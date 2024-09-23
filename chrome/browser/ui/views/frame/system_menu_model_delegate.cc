// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/system_menu_model_delegate.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/grit/generated_resources.h"
#include "components/sessions/core/tab_restore_service.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/ui/frame/desks/move_to_desks_menu_delegate.h"
#include "chromeos/ui/frame/desks/move_to_desks_menu_model.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#endif

SystemMenuModelDelegate::SystemMenuModelDelegate(
    ui::AcceleratorProvider* provider,
    Browser* browser)
    : provider_(provider),
      browser_(browser) {
}

SystemMenuModelDelegate::~SystemMenuModelDelegate() {}

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
  return chrome::IsCommandEnabled(browser_, command_id);
}

bool SystemMenuModelDelegate::IsCommandIdVisible(int command_id) const {
// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
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
  return true;
}

bool SystemMenuModelDelegate::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) const {
  return provider_->GetAcceleratorForCommandId(command_id, accelerator);
}

bool SystemMenuModelDelegate::IsItemForCommandIdDynamic(int command_id) const {
  return command_id == IDC_RESTORE_TAB;
}

std::u16string SystemMenuModelDelegate::GetLabelForCommandId(
    int command_id) const {
  DCHECK_EQ(command_id, IDC_RESTORE_TAB);

  int string_id = IDS_RESTORE_TAB;
  if (IsCommandIdEnabled(command_id)) {
    sessions::TabRestoreService* trs =
        TabRestoreServiceFactory::GetForProfile(browser_->profile());
    DCHECK(trs);
    trs->LoadTabsFromLastSession();
    if (!trs->entries().empty()) {
      if (trs->entries().front()->type == sessions::tab_restore::Type::WINDOW) {
        string_id = IDS_REOPEN_WINDOW;
      } else if (trs->entries().front()->type ==
                 sessions::tab_restore::Type::GROUP) {
        string_id = IDS_REOPEN_GROUP;
      }
    }
  }
  return l10n_util::GetStringUTF16(string_id);
}

void SystemMenuModelDelegate::ExecuteCommand(int command_id, int event_flags) {
  chrome::ExecuteCommand(browser_, command_id);
}
