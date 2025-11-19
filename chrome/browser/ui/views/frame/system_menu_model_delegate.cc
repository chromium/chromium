// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/system_menu_model_delegate.h"

#include "base/metrics/user_metrics.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/core/tab_restore_service.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/ui/frame/desks/move_to_desks_menu_delegate.h"
#include "chromeos/ui/frame/desks/move_to_desks_menu_model.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include "chrome/common/pref_names.h"
#endif

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#endif

SystemMenuModelDelegate::SystemMenuModelDelegate(
    ui::AcceleratorProvider* provider,
    Browser* browser)
    : provider_(provider), browser_(browser) {}

SystemMenuModelDelegate::~SystemMenuModelDelegate() = default;

bool SystemMenuModelDelegate::IsCommandIdChecked(int command_id) const {
#if BUILDFLAG(IS_LINUX)
  if (command_id == IDC_USE_SYSTEM_TITLE_BAR) {
    PrefService* prefs = browser_->profile()->GetPrefs();
    return !prefs->GetBoolean(prefs::kUseCustomChromeFrame);
  }
#endif
  return false;
}

bool SystemMenuModelDelegate::IsCommandIdEnabled(int command_id) const {
#if BUILDFLAG(IS_CHROMEOS)
  if (command_id == chromeos::MoveToDesksMenuModel::kMenuCommandId) {
    return chromeos::MoveToDesksMenuDelegate::ShouldShowMoveToDesksMenu(
        browser_->window()->GetNativeWindow());
  }
#endif
#if BUILDFLAG(ENABLE_GLIC)
  // Disable the glic toggle pin if it is showing and glic is not enabled.
  if (command_id == IDC_GLIC_TOGGLE_PIN) {
    return glic::GlicEnabling::IsEnabledForProfile(browser_->profile());
  }
#endif
  return chrome::IsCommandEnabled(browser_, command_id);
}

bool SystemMenuModelDelegate::IsCommandIdVisible(int command_id) const {
#if BUILDFLAG(IS_LINUX)
  bool is_maximized = browser_->window()->IsMaximized();
  switch (command_id) {
    case IDC_MAXIMIZE_WINDOW:
      return !is_maximized;
    case IDC_RESTORE_WINDOW:
      return is_maximized;
  }
#endif
#if BUILDFLAG(IS_CHROMEOS)
  if (command_id == chromeos::MoveToDesksMenuModel::kMenuCommandId) {
    return chromeos::MoveToDesksMenuDelegate::ShouldShowMoveToDesksMenu(
        browser_->window()->GetNativeWindow());
  }
#endif
#if BUILDFLAG(ENABLE_GLIC)
  if (command_id == IDC_GLIC_TOGGLE_PIN) {
    return glic::GlicEnabling::IsEnabledForProfile(browser_->profile());
  }
#endif
  return true;
}

bool SystemMenuModelDelegate::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) const {
  return provider_->GetAcceleratorForCommandId(command_id, accelerator);
}

bool SystemMenuModelDelegate::IsItemForCommandIdDynamic(int command_id) const {
  return std::set{IDC_RESTORE_TAB, IDC_GLIC_TOGGLE_PIN,
                  IDC_TOGGLE_VERTICAL_TABS}
      .contains(command_id);
}

std::u16string SystemMenuModelDelegate::GetLabelForCommandId(
    int command_id) const {
  DCHECK(IsItemForCommandIdDynamic(command_id));

  int string_id;
  switch (command_id) {
    case IDC_RESTORE_TAB:
      string_id = IDS_RESTORE_TAB;
      if (IsCommandIdEnabled(command_id)) {
        sessions::TabRestoreService* trs =
            TabRestoreServiceFactory::GetForProfile(browser_->profile());
        DCHECK(trs);
        trs->LoadTabsFromLastSession();
        if (!trs->entries().empty()) {
          if (trs->entries().front()->type ==
              sessions::tab_restore::Type::WINDOW) {
            string_id = IDS_REOPEN_WINDOW;
          } else if (trs->entries().front()->type ==
                     sessions::tab_restore::Type::GROUP) {
            string_id = IDS_REOPEN_GROUP;
          }
        }
      }
      break;
    case IDC_TOGGLE_VERTICAL_TABS:
      string_id = browser_->browser_window_features()
                          ->vertical_tab_strip_state_controller()
                          ->ShouldDisplayVerticalTabs()
                      ? IDS_SWITCH_TO_HORIZONTAL_TAB
                      : IDS_SWITCH_TO_VERTICAL_TAB;
      break;
#if BUILDFLAG(ENABLE_GLIC)
    case IDC_GLIC_TOGGLE_PIN:
      string_id = browser_->profile()->GetPrefs()->GetBoolean(
                      glic::prefs::kGlicPinnedToTabstrip)
                      ? IDS_GLIC_UNPIN
                      : IDS_GLIC_PIN;
      break;
#endif
    default:
      NOTREACHED();
  }
  return l10n_util::GetStringUTF16(string_id);
}

void SystemMenuModelDelegate::ExecuteCommand(int command_id, int event_flags) {
  switch (command_id) {
    case IDC_BOOKMARK_ALL_TABS:
      base::RecordAction(
          base::UserMetricsAction("SystemContextMenu_BookmarkAllTabs"));
      break;
    case IDC_NEW_TAB:
      base::RecordAction(base::UserMetricsAction("SystemContextMenu_NewTab"));
      break;
    case IDC_RESTORE_TAB:
      base::RecordAction(
          base::UserMetricsAction("SystemContextMenu_RestoreTab"));
      break;
    case IDC_GROUP_UNGROUPED_TABS:
      base::RecordAction(
          base::UserMetricsAction("SystemContextMenu_GroupAllTabs"));
      break;
    case IDC_NAME_WINDOW:
      base::RecordAction(
          base::UserMetricsAction("SystemContextMenu_NameWindow"));
      break;
  }
  chrome::ExecuteCommand(browser_, command_id);
}
