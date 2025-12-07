// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_footer/footer_context_menu.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/side_panel/side_panel_action_callback.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(FooterContextMenu,
                                      kHideFooterIdForTesting);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(FooterContextMenu,
                                      kShowCustomizeChromeIdForTesting);

namespace new_tab_footer {
void RecordContextMenuClick(FooterContextMenuItem item) {
  base::UmaHistogramEnumeration("NewTabPage.Footer.ContextMenuClicked", item);
}
}  // namespace new_tab_footer

FooterContextMenu::FooterContextMenu(BrowserWindowInterface* browser)
    : ui::SimpleMenuModel(this),
      browser_(browser),
      profile_(browser->GetProfile()) {
  const int icon_size = 16;

  // Add item: close footer.
  AddItemWithIcon(
      COMMAND_CLOSE_FOOTER, l10n_util::GetStringUTF16(IDS_HIDE_NEW_TAB_FOOTER),
      ui::ImageModel::FromVectorIcon(vector_icons::kVisibilityOffIcon,
                                     ui::kColorIcon, icon_size));
  SetElementIdentifierAt(GetIndexOfCommandId(COMMAND_CLOSE_FOOTER).value(),
                         kHideFooterIdForTesting);

  AddSeparator(ui::NORMAL_SEPARATOR);

  // Add item: customize chrome.
  AddItemWithStringIdAndIcon(
      COMMAND_SHOW_CUSTOMIZE_CHROME, IDS_NTP_CUSTOMIZE_BUTTON_LABEL,
      ui::ImageModel::FromVectorIcon(vector_icons::kEditChromeRefreshIcon,
                                     ui::kColorIcon, icon_size));
  SetElementIdentifierAt(
      GetIndexOfCommandId(COMMAND_SHOW_CUSTOMIZE_CHROME).value(),
      kShowCustomizeChromeIdForTesting);
}

FooterContextMenu::~FooterContextMenu() = default;

bool FooterContextMenu::IsCommandIdVisible(int command_id) const {
  switch (command_id) {
    case COMMAND_CLOSE_FOOTER: {
      bool is_controlled_by_policy =
          enterprise_util::GetManagementNoticeStateForNTPFooter(profile_) ==
          enterprise_util::BrowserManagementNoticeState::kEnabledByPolicy;
      return !is_controlled_by_policy;
    };
  }
  return true;
}

void FooterContextMenu::ExecuteCommand(int command_id, int event_flags) {
  switch (command_id) {
    case COMMAND_CLOSE_FOOTER: {
      new_tab_footer::RecordContextMenuClick(
          new_tab_footer::FooterContextMenuItem::kHideFooter);
      profile_->GetPrefs()->SetBoolean(prefs::kNtpFooterVisible, false);
      break;
    }
    case COMMAND_SHOW_CUSTOMIZE_CHROME: {
      new_tab_footer::RecordContextMenuClick(
          new_tab_footer::FooterContextMenuItem::kCustomizeChrome);
      actions::ActionManager::Get()
          .FindAction(kActionSidePanelShowCustomizeChromeFooter,
                      /*scope=*/browser_->GetActions()->root_action_item())
          ->InvokeAction(
              actions::ActionInvocationContext::Builder()
                  .SetProperty(
                      kSidePanelOpenTriggerKey,
                      static_cast<std::underlying_type_t<SidePanelOpenTrigger>>(
                          SidePanelOpenTrigger::kNewTabFooter))
                  .Build());
      break;
    }
  }
}
