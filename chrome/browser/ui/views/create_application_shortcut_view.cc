// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/create_application_shortcut_view.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/web_applications/components/os_integration_manager.h"
#include "chrome/browser/web_applications/extensions/web_app_extension_shortcut.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/prefs/pref_service.h"
#include "extensions/common/extension.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"

#if defined(OS_WIN)
#include "base/win/shortcut.h"
#include "base/win/windows_version.h"
#endif  // defined(OS_WIN)

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
    : CreateChromeApplicationShortcutView(profile, std::move(close_callback)) {
  SetModalType(ui::MODAL_TYPE_WINDOW);
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
    : CreateChromeApplicationShortcutView(profile, std::move(close_callback)) {
  web_app::WebAppProvider* provider = web_app::WebAppProvider::Get(profile);
  provider->os_integration_manager().GetShortcutInfoForApp(
      web_app_id,
      base::BindRepeating(&CreateChromeApplicationShortcutView::OnAppInfoLoaded,
                          weak_ptr_factory_.GetWeakPtr()));
}

CreateChromeApplicationShortcutView::CreateChromeApplicationShortcutView(
    Profile* profile,
    base::OnceCallback<void(bool)> close_callback)
    : profile_(profile), close_callback_(std::move(close_callback)) {
  SetButtonLabel(ui::DIALOG_BUTTON_OK,
                 l10n_util::GetStringUTF16(IDS_CREATE_SHORTCUTS_COMMIT));
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::TEXT, views::TEXT));
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

  chrome::RecordDialogCreation(
      chrome::DialogIdentifier::CREATE_CHROME_APPLICATION_SHORTCUT);
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
  std::unique_ptr<views::Checkbox> quick_launch_check_box;

#if defined(OS_WIN)
  base::win::Version version = base::win::GetVersion();
  // Do not allow creating shortcuts on the Start Screen for Windows 8.
  if (version != base::win::Version::WIN8 &&
      version != base::win::Version::WIN8_1) {
    menu_check_box = AddCheckbox(
        l10n_util::GetStringUTF16(IDS_CREATE_SHORTCUTS_START_MENU_CHKBOX),
        prefs::kWebAppCreateInAppsMenu);
  }

  // Win10 actively prevents creating shortcuts on the taskbar so we eliminate
  // that option from the dialog.
  if (base::win::CanPinShortcutToTaskbar()) {
    quick_launch_check_box =
        AddCheckbox((version >= base::win::Version::WIN7)
                        ? l10n_util::GetStringUTF16(IDS_PIN_TO_TASKBAR_CHKBOX)
                        : l10n_util::GetStringUTF16(
                              IDS_CREATE_SHORTCUTS_QUICK_LAUNCH_BAR_CHKBOX),
                    prefs::kWebAppCreateInQuickLaunchBar);
  }
#elif defined(OS_POSIX)
  menu_check_box =
      AddCheckbox(l10n_util::GetStringUTF16(IDS_CREATE_SHORTCUTS_MENU_CHKBOX),
                  prefs::kWebAppCreateInAppsMenu);
#endif

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  // Layout controls
  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());

  static const int kHeaderColumnSetId = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(kHeaderColumnSetId);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER, 1.0,
                        views::GridLayout::ColumnSize::kFixed, 0, 0);

  static const int kTableColumnSetId = 1;
  column_set = layout->AddColumnSet(kTableColumnSetId);
  column_set->AddPaddingColumn(
      views::GridLayout::kFixedSize,
      provider->GetDistanceMetric(DISTANCE_SUBSECTION_HORIZONTAL_INDENT));
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1.0,
                        views::GridLayout::ColumnSize::kUsePreferred, 0, 0);

  layout->StartRow(views::GridLayout::kFixedSize, kHeaderColumnSetId);
  layout->AddView(std::move(create_shortcuts_label));

  layout->AddPaddingRow(
      views::GridLayout::kFixedSize,
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL));
  layout->StartRow(views::GridLayout::kFixedSize, kTableColumnSetId);
  desktop_check_box_ = layout->AddView(std::move(desktop_check_box));

  const int vertical_spacing =
      provider->GetDistanceMetric(DISTANCE_RELATED_CONTROL_VERTICAL_SMALL);
  if (menu_check_box) {
    layout->AddPaddingRow(views::GridLayout::kFixedSize, vertical_spacing);
    layout->StartRow(views::GridLayout::kFixedSize, kTableColumnSetId);
    menu_check_box_ = layout->AddView(std::move(menu_check_box));
  }

  if (quick_launch_check_box) {
    layout->AddPaddingRow(views::GridLayout::kFixedSize, vertical_spacing);
    layout->StartRow(views::GridLayout::kFixedSize, kTableColumnSetId);
    quick_launch_check_box_ =
        layout->AddView(std::move(quick_launch_check_box));
  }
}

gfx::Size CreateChromeApplicationShortcutView::CalculatePreferredSize() const {
  static const int kDialogWidth = 360;
  int height = GetLayoutManager()->GetPreferredHeightForWidth(this,
      kDialogWidth);
  return gfx::Size(kDialogWidth, height);
}

bool CreateChromeApplicationShortcutView::IsDialogButtonEnabled(
    ui::DialogButton button) const {
  if (button != ui::DIALOG_BUTTON_OK)
    return true;  // It's always possible to cancel out of creating a shortcut.

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
  DCHECK(IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));

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

#if defined(OS_WIN)
  creation_locations.in_quick_launch_bar =
      quick_launch_check_box_ && quick_launch_check_box_->GetChecked();
#elif defined(OS_POSIX)
  // Create shortcut in Mac dock or as Linux (gnome/kde) application launcher
  // are not implemented yet.
  creation_locations.in_quick_launch_bar = false;
#endif

  web_app::CreateShortcutsWithInfo(web_app::SHORTCUT_CREATION_BY_USER,
                                   creation_locations, base::DoNothing(),
                                   std::move(shortcut_info_));
}

std::unique_ptr<views::Checkbox>
CreateChromeApplicationShortcutView::AddCheckbox(const std::u16string& text,
                                                 const std::string& pref_path) {
  auto checkbox =
      std::make_unique<views::Checkbox>(text, views::Button::PressedCallback());
  checkbox->SetCallback(base::BindRepeating(
      &CreateChromeApplicationShortcutView::CheckboxPressed,
      base::Unretained(this), pref_path, base::Unretained(checkbox.get())));
  checkbox->SetChecked(profile_->GetPrefs()->GetBoolean(pref_path));
  return checkbox;
}

void CreateChromeApplicationShortcutView::CheckboxPressed(
    std::string pref_path,
    views::Checkbox* checkbox) {
  profile_->GetPrefs()->SetBoolean(pref_path, checkbox->GetChecked());
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

BEGIN_METADATA(CreateChromeApplicationShortcutView, views::DialogDelegateView)
END_METADATA
