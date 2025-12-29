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
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips.mojom-data-view.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips.mojom-forward.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips_generator.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/tab_id_generator.h"
#include "components/google/core/common/google_util.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/url_row.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/page_content_annotations/core/page_content_annotations_features.h"
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
using ::action_chips::mojom::ActionChip;
using ::action_chips::mojom::ActionChipPtr;
using ::action_chips::mojom::ChipType;
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

// Helper method to record impression metrics for the generated chips.
void RecordImpressionMetrics(const std::vector<ActionChipPtr>& chips) {
  for (const auto& chip : chips) {
    base::UmaHistogramEnumeration("NewTabPage.ActionChips.Shown", chip->type);
  }
}

// Helper method to record latency metrics for action chips retrieval.
void RecordActionChipsRetrievalLatencyMetrics(base::TimeDelta latency) {
  base::UmaHistogramTimes(
      "NewTabPage.ActionChips.Handler.ActionChipsRetrievalLatency", latency);
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
      action_chips_generator_(std::move(action_chips_generator)),
      history_service_(HistoryServiceFactory::GetForProfile(
          profile_,
          ServiceAccessType::IMPLICIT_ACCESS)) {
  content::WebContents* web_contents = web_ui_->GetWebContents();
  auto* browser_window_interface =
      webui::GetBrowserWindowInterface(web_contents);
  // No need to call RemoveObserver later since TabStripModelObserver takes care
  // of it in its destructor.
  browser_window_interface->GetTabStripModel()->AddObserver(this);
}

ActionChipsHandler::~ActionChipsHandler() = default;

void ActionChipsHandler::StartActionChipsRetrieval() {
  const auto start_time = base::TimeTicks::Now();
  if (!page_.is_bound()) {
    return;
  }

  TabInterface* tab = FindMostRecentTab(*web_ui_);

  // Check sensitivity of tab, if tab available and sensitivity checking
  // is available.
  if (ntp_features::kNtpNextClientSensitivityCheckParam.Get() &&
      tab != nullptr &&
      page_content_annotations::features::
          ShouldExecutePageVisibilityModelOnPageContent(
              g_browser_process->GetApplicationLocale()) &&
      history_service_) {
    history::QueryOptions options;
    options.max_count = 1;
    auto tab_url = tab->GetContents()->GetLastCommittedURL().spec();
    history_service_->QueryHistory(
        base::UTF8ToUTF16(tab_url), options,
        base::BindOnce(&ActionChipsHandler::OnGetHistoryData,
                       weak_factory_.GetWeakPtr(), std::move(tab),
                       std::move(start_time)),
        &cancelable_task_tracker_);
    return;
  }

  action_chips_generator_->GenerateActionChips(
      std::move(tab),
      base::BindOnce(&ActionChipsHandler::SendActionChipsToUi,
                     weak_factory_.GetWeakPtr(), std::move(start_time)));
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

void ActionChipsHandler::OnGetHistoryData(const TabInterface* tab,
                                          base::TimeTicks start_time,
                                          history::QueryResults results) {
  bool is_sensitive =
      results.empty() ||
      results[0].content_annotations().model_annotations.visibility_score <=
          0.70;

  action_chips_generator_->GenerateActionChips(
      is_sensitive ? nullptr : std::move(tab),
      base::BindOnce(&ActionChipsHandler::SendActionChipsToUi,
                     weak_factory_.GetWeakPtr(), std::move(start_time)));
}
