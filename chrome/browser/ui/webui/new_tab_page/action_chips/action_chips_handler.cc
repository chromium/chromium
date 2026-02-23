// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips_handler.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips.mojom.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips_generator.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips_metrics.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/tab_id_generator.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_ui.h"
#include "chrome/common/pref_names.h"
#include "components/contextual_search/contextual_search_metrics_recorder.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/google/core/common/google_util.h"
#include "components/search/ntp_features.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/clipboard_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"
#include "url/mojom/url.mojom.h"

namespace {
using ::action_chips::RecordActionChipsRetrievalLatencyMetrics;
using ::action_chips::RecordImpressionMetrics;
using ::action_chips::mojom::ActionChip;
using ::action_chips::mojom::ActionChipPtr;
using ::action_chips::mojom::TabInfo;
using ::action_chips::mojom::TabInfoPtr;
using ::tabs::TabInterface;

/**
 * Helper method to check if a tab is invalid for the Recent Tab Action Chip:
 * - Google Search Results Page (SRP) or AIM SRP
 * - Invalid URL
 * - About Blank
 * - Chrome internal page
 * - Chrome untrusted internal page
 */
bool IsInvalidMostRecentTab(content::WebContents& contents) {
  const GURL& url = contents.GetLastCommittedURL();
  return google_util::IsGoogleSearchUrl(url) || !url.is_valid() ||
         url.IsAboutBlank() || url.SchemeIs(content::kChromeUIScheme) ||
         url.SchemeIs(content::kChromeUIUntrustedScheme);
}

TabInterface* FindMostRecentTab(content::WebUI& web_ui) {
  auto* browser_window_interface =
      webui::GetBrowserWindowInterface(web_ui.GetWebContents());
  if (!browser_window_interface) {
    return nullptr;
  }

  auto* tab_strip_model = browser_window_interface->GetTabStripModel();
  TabInterface* most_recent_tab = nullptr;
  base::Time latest_active_time;

  for (int i = 0; i < tab_strip_model->count(); ++i) {
    TabInterface* tab = tab_strip_model->GetTabAtIndex(i);
    if (tab == most_recent_tab) {
      continue;
    }
    // TabInterface::GetContents returns a non-nullptr according to its comment.
    content::WebContents& contents = *tab->GetContents();
    if (IsInvalidMostRecentTab(contents)) {
      continue;
    }
    const base::Time last_active = contents.GetLastActiveTime();
    if (last_active > latest_active_time) {
      latest_active_time = last_active;
      most_recent_tab = tab;
    }
  }

  return most_recent_tab;
}

bool IsTabReadyForActionChipsRetrieval(content::WebContents* web_contents,
                                       const TabStripModelChange& change) {
  if (web_contents == nullptr) {
    return false;
  }

  if (change.type() == TabStripModelChange::kReplaced &&
      change.GetReplace()->old_contents == web_contents) {
    return false;
  }

  return tabs::TabInterface::GetFromContents(web_contents)->IsActivated();
}
}  // namespace

ActionChipsHandler::ActionChipsHandler(
    mojo::PendingReceiver<action_chips::mojom::ActionChipsHandler> receiver,
    mojo::PendingRemote<action_chips::mojom::Page> page,
    Profile* profile,
    content::WebUI* web_ui,
    std::unique_ptr<ActionChipsGenerator> action_chips_generator)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      profile_(profile),
      web_ui_(web_ui),
      action_chips_generator_(std::move(action_chips_generator)) {
  content::WebContents* web_contents = web_ui_->GetWebContents();
  auto* browser_window_interface =
      webui::GetBrowserWindowInterface(web_contents);
  // No need to call RemoveObserver later since TabStripModelObserver takes care
  // of it in its destructor.
  browser_window_interface->GetTabStripModel()->AddObserver(this);
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kNtpToolChipsVisible,
      base::BindRepeating(&ActionChipsHandler::OnVisibilityChanged,
                          weak_factory_.GetWeakPtr()));
}

ActionChipsHandler::~ActionChipsHandler() = default;

void ActionChipsHandler::StartActionChipsRetrieval() {
  const auto start_time = base::TimeTicks::Now();
  if (!page_.is_bound()) {
    return;
  }

  TabInterface* tab = nullptr;
  if (contextual_search::ContextualSearchService::IsContextSharingEnabled(
          profile_->GetPrefs())) {
    tab = FindMostRecentTab(*web_ui_);
  }

  const GURL current_url =
      tab != nullptr ? tab->GetContents()->GetLastCommittedURL() : GURL();
  if (ShouldThrottleRetrieval(current_url)) {
    return;
  }

  action_chips_generator_->GenerateActionChips(
      std::move(tab),
      base::BindOnce(&ActionChipsHandler::SendActionChipsToUi,
                     weak_factory_.GetWeakPtr(), std::move(start_time)));
}

void ActionChipsHandler::ActivateMetricsFunnel(const std::string& funnel_name) {
  auto* controller = web_ui_->GetController();
  NewTabPageUI* ntp_ui =
      controller ? controller->GetAs<NewTabPageUI>() : nullptr;
  if (!ntp_ui) {
    return;
  }

  auto* session_handle = ntp_ui->GetOrCreateContextualSessionHandle();
  if (!session_handle) {
    return;
  }

  auto* metrics_recorder = session_handle->GetMetricsRecorder();
  if (metrics_recorder) {
    metrics_recorder->ActivateMetricsFunnel(funnel_name);
  }
}

void ActionChipsHandler::SendActionChipsToUi(base::TimeTicks start_time,
                                             std::vector<ActionChipPtr> chips) {
  if (!page_.is_bound()) {
    return;
  }
  if (!ntp_features::kNtpNextShowSimplificationUIParam.Get() &&
      chips.size() <= 1) {
    // We show a chip only when there are more than one chip. This occurs when
    // there is no tab opened and only one of the AIM features are enabled.
    // This branch ensures that no chip is displayed by returning an empty list.
    chips.clear();
  }

  RecordActionChipsRetrievalLatencyMetrics(base::TimeTicks::Now() - start_time);
  RecordImpressionMetrics(chips);
  action_chips::RecordActionChipsAnyShown(!chips.empty());

  page_->OnActionChipsChanged(std::move(chips));
}

void ActionChipsHandler::OnTabStripModelChanged(
    TabStripModel*,
    const TabStripModelChange& change,
    const TabStripSelectionChange&) {
  if (!IsTabReadyForActionChipsRetrieval(web_ui_->GetWebContents(), change)) {
    return;
  }
  StartActionChipsRetrieval();
}

bool ActionChipsHandler::ShouldThrottleRetrieval(const GURL& current_url) {
  if (last_processed_url_ == current_url) {
    return true;
  }
  last_processed_url_ = current_url;
  return false;
}

void ActionChipsHandler::OnVisibilityChanged() {
  if (profile_->GetPrefs()->GetBoolean(prefs::kNtpToolChipsVisible)) {
    last_processed_url_.reset();
    StartActionChipsRetrieval();
  }
}
