// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_footer_panel.h"

#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/app_constants/constants.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/management_policy.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
// gn check complains on Linux Ozone.
#include "ash/public/cpp/shelf_model.h"  // nogncheck
#include "chrome/browser/ui/ash/shelf/app_shortcut_shelf_item_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller_util.h"
#endif

AppInfoFooterPanel::AppInfoFooterPanel(Profile* profile,
                                       const extensions::Extension* app)
    : AppInfoPanel(profile, app) {
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      provider->GetInsetsMetric(views::INSETS_DIALOG_SUBSECTION),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_BUTTON_HORIZONTAL)));

  CreateButtons();
}

AppInfoFooterPanel::~AppInfoFooterPanel() {}

// static
std::unique_ptr<AppInfoFooterPanel> AppInfoFooterPanel::CreateFooterPanel(
    Profile* profile,
    const extensions::Extension* app) {
  if (CanCreateShortcuts(app) ||
#if BUILDFLAG(IS_CHROMEOS_ASH)
      CanSetPinnedToShelf(profile, app) ||
#endif
      CanUninstallApp(profile, app))
    return std::make_unique<AppInfoFooterPanel>(profile, app);
  return nullptr;
}

void AppInfoFooterPanel::CreateButtons() {
  if (CanCreateShortcuts(app_)) {
    create_shortcuts_button_ =
        AddChildView(std::make_unique<views::MdTextButton>(
            base::BindRepeating(&AppInfoFooterPanel::CreateShortcuts,
                                base::Unretained(this)),
            l10n_util::GetStringUTF16(
                IDS_APPLICATION_INFO_CREATE_SHORTCUTS_BUTTON_TEXT)));
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (CanSetPinnedToShelf(profile_, app_)) {
    pin_to_shelf_button_ = AddChildView(std::make_unique<views::MdTextButton>(
        base::BindRepeating(&AppInfoFooterPanel::SetPinnedToShelf,
                            base::Unretained(this), true),
        l10n_util::GetStringUTF16(IDS_APP_LIST_CONTEXT_MENU_PIN)));
    unpin_from_shelf_button_ =
        AddChildView(std::make_unique<views::MdTextButton>(
            base::BindRepeating(&AppInfoFooterPanel::SetPinnedToShelf,
                                base::Unretained(this), false),
            l10n_util::GetStringUTF16(IDS_APP_LIST_CONTEXT_MENU_UNPIN)));
    UpdatePinButtons(false);
  }
#endif

  if (CanUninstallApp(profile_, app_)) {
    remove_button_ = AddChildView(std::make_unique<views::MdTextButton>(
        base::BindRepeating(&AppInfoFooterPanel::UninstallApp,
                            base::Unretained(this)),
        l10n_util::GetStringUTF16(IDS_APPLICATION_INFO_UNINSTALL_BUTTON_TEXT)));
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void AppInfoFooterPanel::UpdatePinButtons(bool focus_visible_button) {
  if (pin_to_shelf_button_ && unpin_from_shelf_button_) {
    const bool was_pinned =
        ChromeShelfController::instance()->shelf_model()->IsAppPinned(
            app_->id());
    pin_to_shelf_button_->SetVisible(!was_pinned);
    unpin_from_shelf_button_->SetVisible(was_pinned);

    if (focus_visible_button) {
      views::View* button_to_focus = was_pinned ? unpin_from_shelf_button_.get()
                                                : pin_to_shelf_button_.get();
      button_to_focus->RequestFocus();
    }
  }
}
#endif

void AppInfoFooterPanel::OnExtensionUninstallDialogClosed(
    bool did_start_uninstall,
    const std::u16string& error) {
  if (did_start_uninstall) {
    // Close the App Info dialog as well (which will free the dialog too).
    Close();
  } else {
    extension_uninstall_dialog_.reset();
  }
}

void AppInfoFooterPanel::CreateShortcuts() {
  DCHECK(CanCreateShortcuts(app_));
  chrome::ShowCreateChromeAppShortcutsDialog(GetWidget()->GetNativeWindow(),
                                             profile_, app_, base::DoNothing());
}

// static
bool AppInfoFooterPanel::CanCreateShortcuts(const extensions::Extension* app) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Ash platforms can't create shortcuts.
  return false;
#else
  // Extensions and the Chrome component app can't have shortcuts.
  return app->id() != app_constants::kChromeAppId && !app->is_extension();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void AppInfoFooterPanel::SetPinnedToShelf(bool value) {
  DCHECK(CanSetPinnedToShelf(profile_, app_));
  ash::ShelfModel* shelf_model =
      ChromeShelfController::instance()->shelf_model();
  DCHECK(shelf_model);
  ash::ShelfModel::ScopedUserTriggeredMutation user_triggered(shelf_model);
  if (value) {
    PinAppWithIDToShelf(app_->id());
  } else {
    UnpinAppWithIDFromShelf(app_->id());
  }

  UpdatePinButtons(true);
  DeprecatedLayoutImmediately();
}

// static
bool AppInfoFooterPanel::CanSetPinnedToShelf(Profile* profile,
                                             const extensions::Extension* app) {
  // The Chrome app can't be unpinned, and extensions can't be pinned.
  return app->id() != app_constants::kChromeAppId && !app->is_extension() &&
         (GetPinnableForAppID(app->id(), profile) ==
          AppListControllerDelegate::PIN_EDITABLE);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

void AppInfoFooterPanel::UninstallApp() {
  DCHECK(CanUninstallApp(profile_, app_));
  extension_uninstall_dialog_ = extensions::ExtensionUninstallDialog::Create(
      profile_, GetWidget()->GetNativeWindow(), this);
  extension_uninstall_dialog_->ConfirmUninstall(
      app_.get(), extensions::UNINSTALL_REASON_USER_INITIATED,
      extensions::UNINSTALL_SOURCE_APP_INFO_DIALOG);
}

// static
bool AppInfoFooterPanel::CanUninstallApp(Profile* profile,
                                         const extensions::Extension* app) {
  extensions::ManagementPolicy* policy =
      extensions::ExtensionSystem::Get(profile)->management_policy();
  return policy->UserMayModifySettings(app, nullptr) &&
         !policy->MustRemainInstalled(app, nullptr);
}

BEGIN_METADATA(AppInfoFooterPanel)
END_METADATA
