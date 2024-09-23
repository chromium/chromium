// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/system_menu_model_builder.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/models/simple_menu_model.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "chromeos/ui/frame/desks/move_to_desks_menu_delegate.h"
#include "chromeos/ui/frame/desks/move_to_desks_menu_model.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/widget/widget.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/multi_user_window_manager.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_helper.h"
#endif

#if BUILDFLAG(IS_OZONE) && !BUILDFLAG(IS_CHROMEOS)
#include "ui/ozone/public/ozone_platform.h"
#endif

SystemMenuModelBuilder::SystemMenuModelBuilder(
    ui::AcceleratorProvider* provider,
    Browser* browser)
    : menu_delegate_(provider, browser) {}

SystemMenuModelBuilder::~SystemMenuModelBuilder() = default;

void SystemMenuModelBuilder::Init() {
  ui::SimpleMenuModel* model = new ui::SimpleMenuModel(&menu_delegate_);
  menu_model_.reset(model);
  BuildMenu(model);
#if BUILDFLAG(IS_WIN)
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
}

void SystemMenuModelBuilder::BuildSystemMenuForBrowserWindow(
    ui::SimpleMenuModel* model) {
// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS_LACROS)
  model->AddItemWithStringId(IDC_MINIMIZE_WINDOW, IDS_MINIMIZE_WINDOW_MENU);
  model->AddItemWithStringId(IDC_MAXIMIZE_WINDOW, IDS_MAXIMIZE_WINDOW_MENU);
  model->AddItemWithStringId(IDC_RESTORE_WINDOW, IDS_RESTORE_WINDOW_MENU);
  model->AddSeparator(ui::NORMAL_SEPARATOR);
#endif
  model->AddItemWithStringId(IDC_NEW_TAB, IDS_NEW_TAB);
  model->AddItemWithStringId(IDC_RESTORE_TAB, IDS_RESTORE_TAB);
  model->AddItemWithStringId(IDC_BOOKMARK_ALL_TABS, IDS_BOOKMARK_ALL_TABS);
  model->AddItemWithStringId(IDC_NAME_WINDOW, IDS_NAME_WINDOW);
  if (base::FeatureList::IsEnabled(features::kCompactMode)) {
    model->AddSeparator(ui::NORMAL_SEPARATOR);
    model->AddItemWithStringId(IDC_COMPACT_MODE, IDS_COMPACT_MODE);
  }
  if (chrome::CanOpenTaskManager()) {
    model->AddSeparator(ui::NORMAL_SEPARATOR);
    model->AddItemWithStringId(IDC_TASK_MANAGER, IDS_TASK_MANAGER);
  }
// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS_LACROS)
  model->AddSeparator(ui::NORMAL_SEPARATOR);
  bool supports_server_side_decorations = true;
#if BUILDFLAG(IS_OZONE) && !BUILDFLAG(IS_CHROMEOS)
  supports_server_side_decorations =
      ui::OzonePlatform::GetInstance()
          ->GetPlatformRuntimeProperties()
          .supports_server_side_window_decorations;
#endif
  if (supports_server_side_decorations) {
    model->AddCheckItemWithStringId(IDC_USE_SYSTEM_TITLE_BAR,
                                    IDS_SHOW_WINDOW_DECORATIONS_MENU);
  }
  model->AddSeparator(ui::NORMAL_SEPARATOR);
  model->AddItemWithStringId(IDC_CLOSE_WINDOW, IDS_CLOSE_WINDOW_MENU);
#endif
#if BUILDFLAG(IS_CHROMEOS)
  AppendMoveToDesksMenu(model);
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
  if (!web_app::AppBrowserController::IsWebApp(browser())) {
    bool is_captive_portal_signin = false;
#if BUILDFLAG(IS_CHROMEOS)
    is_captive_portal_signin =
        browser()->profile()->IsOffTheRecord() &&
        browser()->profile()->GetOTRProfileID().IsCaptivePortal();
#endif
    if (!is_captive_portal_signin) {
      model->AddSeparator(ui::NORMAL_SEPARATOR);
      if (browser()->is_type_app() || browser()->is_type_app_popup()) {
        model->AddItemWithStringId(IDC_NEW_TAB, IDS_APP_MENU_NEW_WEB_PAGE);
      } else {
        model->AddItemWithStringId(IDC_SHOW_AS_TAB, IDS_SHOW_AS_TAB);
      }
    }
    model->AddSeparator(ui::NORMAL_SEPARATOR);
    model->AddItemWithStringId(IDC_CUT, IDS_CUT);
    model->AddItemWithStringId(IDC_COPY, IDS_COPY);
    model->AddItemWithStringId(IDC_PASTE, IDS_PASTE);
    model->AddSeparator(ui::NORMAL_SEPARATOR);
    model->AddItemWithStringId(IDC_FIND, IDS_FIND);
    model->AddItemWithStringId(IDC_PRINT, IDS_PRINT);
    zoom_menu_contents_ =
        std::make_unique<ui::SimpleMenuModel>(&menu_delegate_);
    zoom_menu_contents_->AddItemWithStringId(IDC_ZOOM_PLUS, IDS_ZOOM_PLUS);
    zoom_menu_contents_->AddItemWithStringId(IDC_ZOOM_NORMAL, IDS_ZOOM_NORMAL);
    zoom_menu_contents_->AddItemWithStringId(IDC_ZOOM_MINUS, IDS_ZOOM_MINUS);
    model->AddSubMenuWithStringId(IDC_ZOOM_MENU, IDS_ZOOM_MENU,
                                  zoom_menu_contents_.get());
  }

  bool should_show_task_manager =
      (browser()->is_type_app() || browser()->is_type_app_popup()) &&
      chrome::CanOpenTaskManager();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Hide TaskManager option for the app if it is locked for OnTask. Only
  // relevant for non-web browser scenarios.
  if (browser()->IsLockedForOnTask()) {
    should_show_task_manager = false;
  }
#endif
  if (should_show_task_manager) {
    model->AddSeparator(ui::NORMAL_SEPARATOR);
    model->AddItemWithStringId(IDC_TASK_MANAGER, IDS_TASK_MANAGER);
  }
#if BUILDFLAG(IS_LINUX)
  model->AddSeparator(ui::NORMAL_SEPARATOR);
  model->AddItemWithStringId(IDC_CLOSE_WINDOW, IDS_CLOSE);
#endif
#if BUILDFLAG(IS_CHROMEOS)
  AppendMoveToDesksMenu(model);
#endif
  AppendTeleportMenu(model);
}

#if BUILDFLAG(IS_CHROMEOS)
void SystemMenuModelBuilder::AppendMoveToDesksMenu(ui::SimpleMenuModel* model) {
  gfx::NativeWindow window =
      menu_delegate_.browser()->window()->GetNativeWindow();
  if (!chromeos::MoveToDesksMenuDelegate::ShouldShowMoveToDesksMenu(window))
    return;

  model->AddSeparator(ui::NORMAL_SEPARATOR);
  move_to_desks_model_ = std::make_unique<chromeos::MoveToDesksMenuModel>(
      std::make_unique<chromeos::MoveToDesksMenuDelegate>(
          views::Widget::GetWidgetForNativeWindow(window)));
  model->AddSubMenuWithStringId(chromeos::MoveToDesksMenuModel::kMenuCommandId,
                                IDS_MOVE_TO_DESKS_MENU,
                                move_to_desks_model_.get());
}
#endif

void SystemMenuModelBuilder::AppendTeleportMenu(ui::SimpleMenuModel* model) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
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
  int command_id = IDC_VISIT_DESKTOP_OF_LRU_USER_NEXT;
  for (size_t user_index = 1; user_index < logged_in_users.size();
       ++user_index) {
    if (command_id > IDC_VISIT_DESKTOP_OF_LRU_USER_LAST) {
      break;
    }
    const user_manager::User* user_info = logged_in_users[user_index];
    model->AddItem(
        command_id,
        l10n_util::GetStringFUTF16(
            IDS_VISIT_DESKTOP_OF_LRU_USER, user_info->GetDisplayName(),
            base::ASCIIToUTF16(user_info->GetDisplayEmail())));
    ++command_id;
  }
#endif
}
