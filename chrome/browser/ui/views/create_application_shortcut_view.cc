// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/create_application_shortcut_view.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/web_applications/extensions/web_app_extension_shortcut.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/os_integration/os_integration_sub_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/prefs/pref_service.h"
#include "extensions/common/extension.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/shortcut.h"
#include "chrome/installer/util/taskbar_util.h"
#endif  // BUILDFLAG(IS_WIN)

namespace chrome {

void ShowCreateChromeAppShortcutsDialog(
    gfx::NativeWindow parent_window,
    Profile* profile,
    const extensions::Extension* app,
    base::OnceCallback<void(bool)> close_callback) {
  constrained_window::CreateBrowserModalDialogViews(
      new CreateChromeApplicationShortcutView(profile, app,
                                              std::move(close_callback)),
      parent_window)
      ->Show();
}

void ShowCreateChromeAppShortcutsDialog(
    gfx::NativeWindow parent_window,
    Profile* profile,
    const std::string& web_app_id,
    base::OnceCallback<void(bool)> close_callback) {
  constrained_window::CreateBrowserModalDialogViews(
      new CreateChromeApplicationShortcutView(profile, web_app_id,
                                              std::move(close_callback)),
      parent_window)
      ->Show();
}

}  // namespace chrome

CreateChromeApplicationShortcutView::CreateChromeApplicationShortcutView(
    Profile* profile,
    const extensions::Extension* app,
    base::OnceCallback<void(bool)> close_callback)
    : CreateChromeApplicationShortcutView(profile,
                                          /*is_extension=*/true,
                                          std::move(close_callback)) {
  // Get shortcut and icon information; needed for creating the shortcut.
  web_app::GetShortcutInfoForApp(
      app, profile,
      base::BindRepeating(&CreateChromeApplicationShortcutView::OnAppInfoLoaded,
                          weak_ptr_factory_.GetWeakPtr()));
}

CreateChromeApplicationShortcutView::CreateChromeApplicationShortcutView(
    Profile* profile,
    const std::string& web_app_id,
    base::OnceCallback<void(bool)> close_callback)
    : CreateChromeApplicationShortcutView(profile,
                                          /*is_extension=*/false,
                                          std::move(close_callback)) {
  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForWebApps(profile);
  provider->os_integration_manager().GetShortcutInfoForAppFromRegistrar(
      web_app_id,
      base::BindRepeating(&CreateChromeApplicationShortcutView::OnAppInfoLoaded,
                          weak_ptr_factory_.GetWeakPtr()));
}

CreateChromeApplicationShortcutView::CreateChromeApplicationShortcutView(
    Profile* profile,
    bool is_extension,
    base::OnceCallback<void(bool)> close_callback)
    : profile_(profile),
      prefs_(profile->GetPrefs()),
      is_extension_(is_extension),
      close_callback_(std::move(close_callback)) {
  SetModalType(ui::mojom::ModalType::kWindow);
  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 l10n_util::GetStringUTF16(IDS_CREATE_SHORTCUTS_COMMIT));
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::DialogContentType::kText, views::DialogContentType::kText));
  SetAcceptCallback(
      base::BindOnce(&CreateChromeApplicationShortcutView::OnDialogAccepted,
                     base::Unretained(this)));
  auto canceled = [](CreateChromeApplicationShortcutView* dialog) {
    if (!dialog->close_callback_.is_null())
      std::move(dialog->close_callback_).Run(false);
  };
  SetCancelCallback(base::BindOnce(canceled, base::Unretained(this)));
  SetCloseCallback(base::BindOnce(canceled, base::Unretained(this)));
  InitControls();
}

CreateChromeApplicationShortcutView::~CreateChromeApplicationShortcutView() {}

void CreateChromeApplicationShortcutView::InitControls() {
  auto create_shortcuts_label = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_CREATE_SHORTCUTS_LABEL));
  create_shortcuts_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  create_shortcuts_label->SetMultiLine(true);

  std::unique_ptr<views::Checkbox> desktop_check_box = AddCheckbox(
      l10n_util::GetStringUTF16(IDS_CREATE_SHORTCUTS_DESKTOP_CHKBOX),
      prefs::kWebAppCreateOnDesktop);

  std::unique_ptr<views::Checkbox> menu_check_box;
  std::unique_ptr<views::Checkbox> pin_to_taskbar_checkbox;

#if BUILDFLAG(IS_WIN)
  menu_check_box = AddCheckbox(
      l10n_util::GetStringUTF16(IDS_CREATE_SHORTCUTS_START_MENU_CHKBOX),
      prefs::kWebAppCreateInAppsMenu);

  // Only include the pin-to-taskbar option when running on versions of Windows
  // that support pinning.
  if (CanPinShortcutToTaskbar()) {
    pin_to_taskbar_checkbox =
        AddCheckbox(l10n_util::GetStringUTF16(IDS_PIN_TO_TASKBAR_CHKBOX),
                    prefs::kWebAppCreateInQuickLaunchBar);
  }
#elif BUILDFLAG(IS_POSIX)
  menu_check_box =
      AddCheckbox(l10n_util::GetStringUTF16(IDS_CREATE_SHORTCUTS_MENU_CHKBOX),
                  prefs::kWebAppCreateInAppsMenu);
#endif

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));
  AddChildView(std::move(create_shortcuts_label));
  desktop_check_box_ = AddChildView(std::move(desktop_check_box));
  if (menu_check_box)
    menu_check_box_ = AddChildView(std::move(menu_check_box));
  if (pin_to_taskbar_checkbox)
    quick_launch_check_box_ = AddChildView(std::move(pin_to_taskbar_checkbox));
}

gfx::Size CreateChromeApplicationShortcutView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  static const int kDialogWidth = 360;
  int height = GetLayoutManager()->GetPreferredHeightForWidth(this,
      kDialogWidth);
  return gfx::Size(kDialogWidth, height);
}

bool CreateChromeApplicationShortcutView::IsDialogButtonEnabled(
    ui::mojom::DialogButton button) const {
  if (button != ui::mojom::DialogButton::kOk) {
    return true;  // It's always possible to cancel out of creating a shortcut.
  }

  if (!shortcut_info_)
    return false;  // Dialog's not ready because app info hasn't been loaded.

  // One of the three location checkboxes must be checked:
  return desktop_check_box_->GetChecked() ||
         (menu_check_box_ && menu_check_box_->GetChecked()) ||
         (quick_launch_check_box_ && quick_launch_check_box_->GetChecked());
}

std::u16string CreateChromeApplicationShortcutView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_CREATE_SHORTCUTS_TITLE);
}

void CreateChromeApplicationShortcutView::OnDialogAccepted() {
  DCHECK(IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));

  if (!close_callback_.is_null())
    std::move(close_callback_).Run(/*success=*/shortcut_info_ != nullptr);

  // Shortcut can't be created because app info hasn't been loaded.
  if (!shortcut_info_)
    return;

  web_app::ShortcutLocations creation_locations;
  creation_locations.on_desktop = desktop_check_box_->GetChecked();
  if (menu_check_box_ && menu_check_box_->GetChecked()) {
    creation_locations.applications_menu_location =
        web_app::APP_MENU_LOCATION_SUBDIR_CHROMEAPPS;
  }

#if BUILDFLAG(IS_WIN)
  creation_locations.in_quick_launch_bar =
      quick_launch_check_box_ && quick_launch_check_box_->GetChecked();
#elif BUILDFLAG(IS_POSIX)
  // Create shortcut in Mac dock or as Linux (gnome/kde) application launcher
  // are not implemented yet.
  creation_locations.in_quick_launch_bar = false;
#endif

  // If the dialog has been triggered from a web_app, then we need to perform OS
  // integration using sub managers so that shortcuts can be properly added,
  // updated or deleted. Otherwise, shortcuts created need not be tracked as
  // they will not be tied to an app_id.
  if (!shortcut_info_->app_id.empty() && !is_extension_) {
    auto* provider = web_app::WebAppProvider::GetForWebApps(profile_);
    CHECK(provider);
    provider->scheduler().SynchronizeOsIntegration(
        shortcut_info_->app_id, base::DoNothing(),
        web_app::ConvertShortcutLocationsToSynchronizeOptions(
            creation_locations, web_app::SHORTCUT_CREATION_BY_USER),
        /*upgrade_to_fully_installed_if_installed=*/true);
  } else {
    web_app::CreateShortcutsWithInfo(web_app::SHORTCUT_CREATION_BY_USER,
                                     creation_locations, base::DoNothing(),
                                     std::move(shortcut_info_));
  }
}

std::unique_ptr<views::Checkbox>
CreateChromeApplicationShortcutView::AddCheckbox(const std::u16string& text,
                                                 const std::string& pref_path) {
  auto checkbox =
      std::make_unique<views::Checkbox>(text, views::Button::PressedCallback());
  checkbox->SetCallback(base::BindRepeating(
      &CreateChromeApplicationShortcutView::CheckboxPressed,
      base::Unretained(this), pref_path, base::Unretained(checkbox.get())));
  checkbox->SetChecked(prefs_->GetBoolean(pref_path));
  return checkbox;
}

void CreateChromeApplicationShortcutView::CheckboxPressed(
    std::string pref_path,
    views::Checkbox* checkbox) {
  prefs_->SetBoolean(pref_path, checkbox->GetChecked());
  DialogModelChanged();
}

void CreateChromeApplicationShortcutView::OnAppInfoLoaded(
    std::unique_ptr<web_app::ShortcutInfo> shortcut_info) {
  // GetShortcutInfoForApp request may return nullptr |shortcut_info| to this
  // callback if web app was uninstalled during that asynchronous request.
  shortcut_info_ = std::move(shortcut_info);
  // This may cause there to be shortcut info when there was none before, so
  // make sure the accept button gets enabled.
  DialogModelChanged();
}

BEGIN_METADATA(CreateChromeApplicationShortcutView)
END_METADATA
