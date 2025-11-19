// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips_handler.h"

#include <utility>
#include <vector>

#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips.mojom-data-view.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips.mojom-forward.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/tab_id_generator.h"
#include "chrome/grit/generated_resources.h"
#include "components/google/core/common/google_util.h"
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

ActionChipPtr CreateRecentTabChip(TabInfoPtr tab) {
  ActionChipPtr chip = ActionChip::New();
  chip->type = ChipType::kRecentTab;
  chip->title = tab->title;
  chip->suggestion = l10n_util::GetStringUTF8(IDS_NTP_ACTION_CHIP_TAB_BODY_1);
  chip->tab = std::move(tab);
  return chip;
}

ActionChipPtr CreateDeepSearchChip() {
  ActionChipPtr chip = ActionChip::New();
  chip->type = ChipType::kDeepSearch;
  chip->title =
      l10n_util::GetStringUTF8(IDS_NTP_ACTION_CHIP_DEEP_SEARCH_HEADING);
  chip->suggestion =
      l10n_util::GetStringUTF8(IDS_NTP_ACTION_CHIP_DEEP_SEARCH_BODY);
  return chip;
}

ActionChipPtr CreateImageCreationChip() {
  ActionChipPtr chip = ActionChip::New();
  chip->type = ChipType::kImage;
  chip->title =
      l10n_util::GetStringUTF8(IDS_NTP_ACTION_CHIP_CREATE_IMAGE_HEADING);
  chip->suggestion =
      l10n_util::GetStringUTF8(IDS_NTP_ACTION_CHIP_CREATE_IMAGE_BODY_1);
  return chip;
}

TabInfoPtr CreateTabInfo(const TabIdGenerator& tab_id_generator,
                         const TabInterface& tab) {
  TabInfoPtr tab_info = action_chips::mojom::TabInfo::New();
  tab_info->tab_id = tab_id_generator.GenerateTabHandleId(&tab);
  content::WebContents& contents = *tab.GetContents();
  tab_info->title = base::UTF16ToUTF8(contents.GetTitle());
  tab_info->url = contents.GetLastCommittedURL();
  tab_info->last_active_time = contents.GetLastActiveTime();
  return tab_info;
}

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
}  // namespace

ActionChipsHandler::ActionChipsHandler(
    mojo::PendingReceiver<action_chips::mojom::ActionChipsHandler> receiver,
    mojo::PendingRemote<action_chips::mojom::Page> page,
    Profile* profile,
    content::WebUI* web_ui,
    const TabIdGenerator* tab_id_generator,
    const TabReadinessChecker* checker)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      profile_(profile),
      web_ui_(web_ui),
      tab_id_generator_(tab_id_generator),
      tab_readiness_checker_(checker) {
  content::WebContents* web_contents = web_ui_->GetWebContents();
  auto* browser_window_interface =
      webui::GetBrowserWindowInterface(web_contents);
  // No need to call RemoveObserver later since TabStripModelObserver takes care
  // of it in its destructor.
  browser_window_interface->GetTabStripModel()->AddObserver(this);
}

ActionChipsHandler::~ActionChipsHandler() = default;

void ActionChipsHandler::StartActionChipsRetrieval() {
  if (!page_.is_bound()) {
    return;
  }
  const TabInterface* tab = FindMostRecentTab(*web_ui_);
  std::vector<ActionChipPtr> chips;
  if (tab != nullptr) {
    chips.push_back(
        CreateRecentTabChip(CreateTabInfo(*this->tab_id_generator_, *tab)));
  }
  chips.push_back(CreateDeepSearchChip());
  chips.push_back(CreateImageCreationChip());

  RecordImpressionMetrics(chips);

  page_->OnActionChipsChanged(std::move(chips));
}

void ActionChipsHandler::OnTabStripModelChanged(
    TabStripModel*,
    const TabStripModelChange&,
    const TabStripSelectionChange&) {
  if (!tab_readiness_checker_->IsReadyForActionChipsRetrieval(
          web_ui_->GetWebContents())) {
    return;
  }
  StartActionChipsRetrieval();
}
