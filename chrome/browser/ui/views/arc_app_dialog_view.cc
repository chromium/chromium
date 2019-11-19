// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/arc/arc_app_dialog.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/arc/arc_app_icon_loader.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/app_list/arc/arc_usb_host_permission_manager.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace arc {

namespace {

const int kArcAppIconSize = 64;
// Currenty ARC apps only support 48*48 native icon.
const int kIconSourceSize = 48;

using ArcAppConfirmCallback = base::OnceCallback<void(bool accept)>;

class ArcAppDialogView : public views::DialogDelegateView,
                         public AppIconLoaderDelegate {
 public:
  ArcAppDialogView(Profile* profile,
                   AppListControllerDelegate* controller,
                   const std::string& app_id,
                   const base::string16& window_title,
                   const base::string16& heading_text,
                   const base::string16& subheading_text,
                   const base::string16& confirm_button_text,
                   const base::string16& cancel_button_text,
                   ArcAppConfirmCallback confirm_callback);
  ~ArcAppDialogView() override;

  // Public method used for test only.
  void ConfirmOrCancelForTest(bool confirm);

 private:
  // views::WidgetDelegate:
  base::string16 GetWindowTitle() const override;
  ui::ModalType GetModalType() const override;
  bool ShouldShowCloseButton() const override;

  // views::DialogDelegate:
  bool Accept() override;
  bool Cancel() override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;

  // AppIconLoaderDelegate:
  void OnAppImageUpdated(const std::string& app_id,
                         const gfx::ImageSkia& image) override;

  void AddMultiLineLabel(views::View* parent, const base::string16& label_text);

  views::ImageView* icon_view_ = nullptr;

  std::unique_ptr<ArcAppIconLoader> icon_loader_;

  Profile* const profile_;

  const std::string app_id_;
  const base::string16 window_title_;
  ArcAppConfirmCallback confirm_callback_;

  DISALLOW_COPY_AND_ASSIGN(ArcAppDialogView);
};

// Browsertest use only. Global pointer of currently shown ArcAppDialogView.
ArcAppDialogView* g_current_arc_app_dialog_view = nullptr;

ArcAppDialogView::ArcAppDialogView(Profile* profile,
                                   AppListControllerDelegate* controller,
                                   const std::string& app_id,
                                   const base::string16& window_title,
                                   const base::string16& heading_text,
                                   const base::string16& subheading_text,
                                   const base::string16& confirm_button_text,
                                   const base::string16& cancel_button_text,
                                   ArcAppConfirmCallback confirm_callback)
    : profile_(profile),
      app_id_(app_id),
      window_title_(window_title),
      confirm_callback_(std::move(confirm_callback)) {
  DialogDelegate::set_button_label(ui::DIALOG_BUTTON_OK, confirm_button_text);
  DialogDelegate::set_button_label(ui::DIALOG_BUTTON_CANCEL,
                                   cancel_button_text);

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      provider->GetDialogInsetsForContentType(views::TEXT, views::TEXT),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_HORIZONTAL)));

  auto icon_view = std::make_unique<views::ImageView>();
  icon_view->SetPreferredSize(gfx::Size(kArcAppIconSize, kArcAppIconSize));
  icon_view_ = AddChildView(std::move(icon_view));

  auto text_container = std::make_unique<views::View>();
  auto text_container_layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical);
  text_container_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  text_container_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  text_container->SetLayoutManager(std::move(text_container_layout));

  auto* text_container_ptr = AddChildView(std::move(text_container));
  DCHECK(!heading_text.empty());
  AddMultiLineLabel(text_container_ptr, heading_text);
  if (!subheading_text.empty())
    AddMultiLineLabel(text_container_ptr, subheading_text);

  // The icon should be loaded synchronously (i.e. OnAppImageUpdated() will be
  // directly called).
  icon_loader_ = std::make_unique<ArcAppIconLoader>(
      profile_, kIconSourceSize, this);
  icon_loader_->FetchImage(app_id_);
  DCHECK(!icon_view_->GetImage().isNull());

  g_current_arc_app_dialog_view = this;
  gfx::NativeWindow parent =
      controller ? controller->GetAppListWindow() : nullptr;
  constrained_window::CreateBrowserModalDialogViews(this, parent)->Show();

  chrome::RecordDialogCreation(chrome::DialogIdentifier::ARC_APP);
}

ArcAppDialogView::~ArcAppDialogView() {
  if (g_current_arc_app_dialog_view == this)
    g_current_arc_app_dialog_view = nullptr;
}

void ArcAppDialogView::AddMultiLineLabel(views::View* parent,
                                         const base::string16& label_text) {
  auto label = std::make_unique<views::Label>(label_text);
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetAllowCharacterBreak(true);
  parent->AddChildView(std::move(label));
}

void ArcAppDialogView::ConfirmOrCancelForTest(bool confirm) {
  if (confirm)
    Accept();
  else
    Cancel();
  GetWidget()->Close();
}

base::string16 ArcAppDialogView::GetWindowTitle() const {
  return window_title_;
}

ui::ModalType ArcAppDialogView::GetModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

bool ArcAppDialogView::ShouldShowCloseButton() const {
  return false;
}

bool ArcAppDialogView::Accept() {
  if (confirm_callback_)
    std::move(confirm_callback_).Run(true);
  return true;
}

bool ArcAppDialogView::Cancel() {
  if (confirm_callback_)
    std::move(confirm_callback_).Run(false);
  return true;
}

gfx::Size ArcAppDialogView::CalculatePreferredSize() const {
  const int default_width = views::LayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH);
  return gfx::Size(default_width, GetHeightForWidth(default_width));
}

void ArcAppDialogView::OnAppImageUpdated(const std::string& app_id,
                                         const gfx::ImageSkia& image) {
  DCHECK_EQ(app_id, app_id_);
  DCHECK(!image.isNull());
  DCHECK_EQ(image.width(), kIconSourceSize);
  DCHECK_EQ(image.height(), kIconSourceSize);
  icon_view_->SetImageSize(image.size());
  icon_view_->SetImage(image);
}

void HandleArcAppUninstall(base::OnceClosure closure, bool accept) {
  if (accept)
    std::move(closure).Run();
}

std::unique_ptr<ArcAppListPrefs::AppInfo> GetArcAppInfo(
    Profile* profile,
    const std::string& app_id) {
  ArcAppListPrefs* arc_prefs = ArcAppListPrefs::Get(profile);
  DCHECK(arc_prefs);
  return arc_prefs->GetApp(app_id);
}

}  // namespace

void ShowArcAppUninstallDialog(Profile* profile,
                               AppListControllerDelegate* controller,
                               const std::string& app_id) {
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      GetArcAppInfo(profile, app_id);
  if (!app_info)
    return;

  bool is_shortcut = app_info->shortcut;

  base::string16 window_title = l10n_util::GetStringUTF16(
      is_shortcut ? IDS_EXTENSION_UNINSTALL_PROMPT_TITLE
                  : IDS_APP_UNINSTALL_PROMPT_TITLE);

  base::string16 heading_text = base::UTF8ToUTF16(l10n_util::GetStringFUTF8(
      is_shortcut ? IDS_EXTENSION_UNINSTALL_PROMPT_HEADING
                  : IDS_NON_PLATFORM_APP_UNINSTALL_PROMPT_HEADING,
      base::UTF8ToUTF16(app_info->name)));
  base::string16 subheading_text;
  if (!is_shortcut) {
    subheading_text = l10n_util::GetStringUTF16(
        IDS_ARC_APP_UNINSTALL_PROMPT_DATA_REMOVAL_WARNING);
  }

  base::string16 confirm_button_text = l10n_util::GetStringUTF16(
      is_shortcut ? IDS_EXTENSION_PROMPT_UNINSTALL_BUTTON
                  : IDS_EXTENSION_PROMPT_UNINSTALL_APP_BUTTON);

  base::string16 cancel_button_text = l10n_util::GetStringUTF16(IDS_CANCEL);
  new ArcAppDialogView(
      profile, controller, app_id, window_title, heading_text, subheading_text,
      confirm_button_text, cancel_button_text,
      base::BindOnce(HandleArcAppUninstall,
                     base::BindOnce(UninstallArcApp, app_id, profile)));
}

void ShowUsbScanDeviceListPermissionDialog(Profile* profile,
                                           const std::string& app_id,
                                           ArcUsbConfirmCallback callback) {
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      GetArcAppInfo(profile, app_id);
  if (!app_info) {
    std::move(callback).Run(false);
    return;
  }

  base::string16 window_title =
      l10n_util::GetStringUTF16(IDS_ARC_USB_PERMISSION_TITLE);

  base::string16 heading_text = l10n_util::GetStringFUTF16(
      IDS_ARC_USB_SCAN_DEVICE_LIST_PERMISSION_HEADING,
      base::UTF8ToUTF16(app_info->name));

  base::string16 confirm_button_text = l10n_util::GetStringUTF16(IDS_OK);

  base::string16 cancel_button_text = l10n_util::GetStringUTF16(IDS_CANCEL);

  new ArcAppDialogView(profile, nullptr /* controller */, app_id, window_title,
                       heading_text, base::string16() /*subheading_text*/,
                       confirm_button_text, cancel_button_text,
                       std::move(callback));
}

void ShowUsbAccessPermissionDialog(Profile* profile,
                                   const std::string& app_id,
                                   const base::string16& device_name,
                                   ArcUsbConfirmCallback callback) {
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      GetArcAppInfo(profile, app_id);
  if (!app_info) {
    std::move(callback).Run(false);
    return;
  }

  base::string16 window_title =
      l10n_util::GetStringUTF16(IDS_ARC_USB_PERMISSION_TITLE);

  base::string16 heading_text = l10n_util::GetStringFUTF16(
      IDS_ARC_USB_ACCESS_PERMISSION_HEADING, base::UTF8ToUTF16(app_info->name));

  base::string16 subheading_text = device_name;

  base::string16 confirm_button_text = l10n_util::GetStringUTF16(IDS_OK);

  base::string16 cancel_button_text = l10n_util::GetStringUTF16(IDS_CANCEL);

  new ArcAppDialogView(profile, nullptr /* controller */, app_id, window_title,
                       heading_text, subheading_text, confirm_button_text,
                       cancel_button_text, std::move(callback));
}

bool IsArcAppDialogViewAliveForTest() {
  return g_current_arc_app_dialog_view != nullptr;
}

bool CloseAppDialogViewAndConfirmForTest(bool confirm) {
  if (!g_current_arc_app_dialog_view)
    return false;

  g_current_arc_app_dialog_view->ConfirmOrCancelForTest(confirm);
  return true;
}

}  // namespace arc
