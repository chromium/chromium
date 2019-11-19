// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/system_menu_model_builder.h"

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/models/simple_menu_model.h"

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/multi_user_window_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_helper.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_info.h"
#include "components/user_manager/user_manager.h"
#include "ui/base/l10n/l10n_util.h"
#endif

SystemMenuModelBuilder::SystemMenuModelBuilder(
    ui::AcceleratorProvider* provider,
    Browser* browser)
    : menu_delegate_(provider, browser) {
}

SystemMenuModelBuilder::~SystemMenuModelBuilder() {
}

void SystemMenuModelBuilder::Init() {
  ui::SimpleMenuModel* model = new ui::SimpleMenuModel(&menu_delegate_);
  menu_model_.reset(model);
  BuildMenu(model);
#if defined(OS_WIN)
  // On Windows we put the menu items in the system menu (not at the end). Doing
  // this necessitates adding a trailing separator.
  model->AddSeparator(ui::NORMAL_SEPARATOR);
#endif
}

void SystemMenuModelBuilder::BuildMenu(ui::SimpleMenuModel* model) {
  // We add the menu items in reverse order so that insertion_index never needs
  // to change.
  if (browser()->is_type_normal())
    BuildSystemMenuForBrowserWindow(model);
  else
    BuildSystemMenuForAppOrPopupWindow(model);
  AddFrameToggleItems(model);
}

void SystemMenuModelBuilder::BuildSystemMenuForBrowserWindow(
    ui::SimpleMenuModel* model) {
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  model->AddItemWithStringId(IDC_MINIMIZE_WINDOW, IDS_MINIMIZE_WINDOW_MENU);
  model->AddItemWithStringId(IDC_MAXIMIZE_WINDOW, IDS_MAXIMIZE_WINDOW_MENU);
  model->AddItemWithStringId(IDC_RESTORE_WINDOW, IDS_RESTORE_WINDOW_MENU);
  model->AddSeparator(ui::NORMAL_SEPARATOR);
#endif
  model->AddItemWithStringId(IDC_NEW_TAB, IDS_NEW_TAB);
  model->AddItemWithStringId(IDC_RESTORE_TAB, IDS_RESTORE_TAB);
  model->AddItemWithStringId(IDC_BOOKMARK_ALL_TABS, IDS_BOOKMARK_ALL_TABS);
  if (chrome::CanOpenTaskManager()) {
    model->AddSeparator(ui::NORMAL_SEPARATOR);
    model->AddItemWithStringId(IDC_TASK_MANAGER, IDS_TASK_MANAGER);
  }
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  model->AddSeparator(ui::NORMAL_SEPARATOR);
  model->AddCheckItemWithStringId(IDC_USE_SYSTEM_TITLE_BAR,
                                  IDS_SHOW_WINDOW_DECORATIONS_MENU);
  model->AddSeparator(ui::NORMAL_SEPARATOR);
  model->AddItemWithStringId(IDC_CLOSE_WINDOW, IDS_CLOSE_WINDOW_MENU);
#endif
  AppendTeleportMenu(model);
  // If it's a regular browser window with tabs, we don't add any more items,
  // since it already has menus (Page, Chrome).
}

void SystemMenuModelBuilder::BuildSystemMenuForAppOrPopupWindow(
    ui::SimpleMenuModel* model) {
  model->AddItemWithStringId(IDC_BACK, IDS_CONTENT_CONTEXT_BACK);
  model->AddItemWithStringId(IDC_FORWARD, IDS_CONTENT_CONTEXT_FORWARD);
  model->AddItemWithStringId(IDC_RELOAD, IDS_APP_MENU_RELOAD);
  if (!web_app::AppBrowserController::IsForWebAppBrowser(browser())) {
    model->AddSeparator(ui::NORMAL_SEPARATOR);
    if (browser()->deprecated_is_app())
      model->AddItemWithStringId(IDC_NEW_TAB, IDS_APP_MENU_NEW_WEB_PAGE);
    else
      model->AddItemWithStringId(IDC_SHOW_AS_TAB, IDS_SHOW_AS_TAB);
    model->AddSeparator(ui::NORMAL_SEPARATOR);
    model->AddItemWithStringId(IDC_CUT, IDS_CUT);
    model->AddItemWithStringId(IDC_COPY, IDS_COPY);
    model->AddItemWithStringId(IDC_PASTE, IDS_PASTE);
    model->AddSeparator(ui::NORMAL_SEPARATOR);
    model->AddItemWithStringId(IDC_FIND, IDS_FIND);
    model->AddItemWithStringId(IDC_PRINT, IDS_PRINT);
    zoom_menu_contents_ = std::make_unique<ZoomMenuModel>(&menu_delegate_);
    model->AddSubMenuWithStringId(IDC_ZOOM_MENU, IDS_ZOOM_MENU,
                                  zoom_menu_contents_.get());
  }
  if (browser()->deprecated_is_app() && chrome::CanOpenTaskManager()) {
    model->AddSeparator(ui::NORMAL_SEPARATOR);
    model->AddItemWithStringId(IDC_TASK_MANAGER, IDS_TASK_MANAGER);
  }
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  model->AddSeparator(ui::NORMAL_SEPARATOR);
  model->AddItemWithStringId(IDC_CLOSE_WINDOW, IDS_CLOSE);
#endif
  AppendTeleportMenu(model);
}

void SystemMenuModelBuilder::AddFrameToggleItems(ui::SimpleMenuModel* model) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDebugEnableFrameToggle)) {
    model->AddSeparator(ui::NORMAL_SEPARATOR);
    model->AddItem(IDC_DEBUG_FRAME_TOGGLE,
                   base::ASCIIToUTF16("Toggle Frame Type"));
  }
}

void SystemMenuModelBuilder::AppendTeleportMenu(ui::SimpleMenuModel* model) {
#if defined(OS_CHROMEOS)
  DCHECK(browser()->window());

  // Avoid appending the teleport menu for the settings window.  This window's
  // presentation is unique: it's a normal browser window with an app-like
  // frame, which doesn't have a user icon badge.  Thus if teleported it's not
  // clear what user it applies to. Rather than bother to implement badging just
  // for this rare case, simply prevent the user from teleporting the window.
  if (chrome::SettingsWindowManager::GetInstance()->IsSettingsBrowser(
          browser())) {
    return;
  }

  // Don't show the menu for incognito windows.
  if (browser()->profile()->IsOffTheRecord())
    return;

  // To show the menu we need at least two logged in users.
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  const user_manager::UserList logged_in_users =
      user_manager->GetLRULoggedInUsers();
  if (logged_in_users.size() <= 1u)
    return;

  // If this does not belong to a profile or there is no window, or the window
  // is not owned by anyone, we don't show the menu addition.
  auto* window_manager = MultiUserWindowManagerHelper::GetWindowManager();
  const AccountId account_id =
      multi_user_util::GetAccountIdFromProfile(browser()->profile());
  aura::Window* window = browser()->window()->GetNativeWindow();
  if (!account_id.is_valid() || !window ||
      !window_manager->GetWindowOwner(window).is_valid())
    return;

  model->AddSeparator(ui::NORMAL_SEPARATOR);
  DCHECK_LE(logged_in_users.size(), 3u);
  for (size_t user_index = 1; user_index < logged_in_users.size();
       ++user_index) {
    const user_manager::UserInfo* user_info = logged_in_users[user_index];
    model->AddItem(
        user_index == 1 ? IDC_VISIT_DESKTOP_OF_LRU_USER_2
                        : IDC_VISIT_DESKTOP_OF_LRU_USER_3,
        l10n_util::GetStringFUTF16(
            IDS_VISIT_DESKTOP_OF_LRU_USER, user_info->GetDisplayName(),
            base::ASCIIToUTF16(user_info->GetDisplayEmail())));
  }
#endif
}
