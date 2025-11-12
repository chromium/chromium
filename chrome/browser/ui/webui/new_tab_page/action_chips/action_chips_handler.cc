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
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/clipboard_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"
#include "url/mojom/url.mojom.h"

namespace {
using ::action_chips::mojom::ActionChip;
using ::action_chips::mojom::ActionChipPtr;
using ::action_chips::mojom::ChipType;
using ::action_chips::mojom::TabInfo;
using ::action_chips::mojom::TabInfoPtr;

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
                         content::WebContents& contents) {
  TabInfoPtr tab_info = action_chips::mojom::TabInfo::New();
  tab_info->tab_id = tab_id_generator.GenerateTabId(&contents).id();
  tab_info->title = base::UTF16ToUTF8(contents.GetTitle());
  tab_info->url = contents.GetLastCommittedURL();
  tab_info->last_active_time = contents.GetLastActiveTime();
  return tab_info;
}

content::WebContents* FindMostRecentTab(content::WebUI& web_ui) {
  content::WebContents* web_contents = web_ui.GetWebContents();
  auto* browser_window_interface =
      webui::GetBrowserWindowInterface(web_contents);
  if (!browser_window_interface) {
    return nullptr;
  }

  auto* tab_strip_model = browser_window_interface->GetTabStripModel();
  content::WebContents* most_recent_contents = nullptr;
  base::Time latest_active_time;

  for (int i = 0; i < tab_strip_model->count(); ++i) {
    content::WebContents* contents = tab_strip_model->GetWebContentsAt(i);
    if (contents == web_contents) {
      continue;
    }
    if (contents->GetLastCommittedURL().SchemeIs(content::kChromeUIScheme)) {
      continue;
    }
    if (contents->GetLastCommittedURL().IsAboutBlank()) {
      continue;
    }
    base::Time last_active = contents->GetLastActiveTime();
    if (last_active > latest_active_time) {
      latest_active_time = last_active;
      most_recent_contents = contents;
    }
  }

  return most_recent_contents;
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
    Profile* profile,
    content::WebUI* web_ui,
    const TabIdGenerator* tab_id_generator)
    : receiver_(this, std::move(receiver)),
      profile_(profile),
      web_ui_(web_ui),
      tab_id_generator_(tab_id_generator) {}

ActionChipsHandler::~ActionChipsHandler() = default;

void ActionChipsHandler::GetActionChips(
    base::OnceCallback<void(std::vector<action_chips::mojom::ActionChipPtr>)>
        callback) {
  content::WebContents* contents = FindMostRecentTab(*web_ui_);
  std::vector<ActionChipPtr> chips;
  if (contents != nullptr) {
    chips.push_back(CreateRecentTabChip(
        CreateTabInfo(*this->tab_id_generator_, *contents)));
  }
  chips.push_back(CreateDeepSearchChip());
  chips.push_back(CreateImageCreationChip());

  RecordImpressionMetrics(chips);

  std::move(callback).Run(std::move(chips));
}
