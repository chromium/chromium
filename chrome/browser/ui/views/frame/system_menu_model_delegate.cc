// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/system_menu_model_delegate.h"

#include "ash/public/cpp/desks_helper.h"
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

// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
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
// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  if (command_id == IDC_USE_SYSTEM_TITLE_BAR) {
    PrefService* prefs = browser_->profile()->GetPrefs();
    return !prefs->GetBoolean(prefs::kUseCustomChromeFrame);
  }
#endif
  return false;
}

bool SystemMenuModelDelegate::IsCommandIdEnabled(int command_id) const {
  return chrome::IsCommandEnabled(browser_, command_id);
}

bool SystemMenuModelDelegate::IsCommandIdVisible(int command_id) const {
// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  bool is_maximized = browser_->window()->IsMaximized();
  switch (command_id) {
    case IDC_MAXIMIZE_WINDOW:
      return !is_maximized;
    case IDC_RESTORE_WINDOW:
      return is_maximized;
  }
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (command_id == IDC_MOVE_TO_DESKS_MENU) {
    auto* desks_helper = ash::DesksHelper::Get();
    return desks_helper && desks_helper->GetNumberOfDesks() > 1;
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
      if (trs->entries().front()->type == sessions::TabRestoreService::WINDOW) {
        string_id = IDS_RESTORE_WINDOW;
      } else if (trs->entries().front()->type ==
                 sessions::TabRestoreService::GROUP) {
        string_id = IDS_RESTORE_GROUP;
      }
    }
  }
  return l10n_util::GetStringUTF16(string_id);
}

void SystemMenuModelDelegate::ExecuteCommand(int command_id, int event_flags) {
  chrome::ExecuteCommand(browser_, command_id);
}
