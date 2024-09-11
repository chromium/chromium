// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ash/app_list/app_service/app_service_app_icon_loader.h"
#include "chrome/browser/ash/app_list/arc/arc_app_dialog.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/app_list/arc/arc_usb_host_permission_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
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
  METADATA_HEADER(ArcAppDialogView, views::DialogDelegateView)

 public:
  ArcAppDialogView(Profile* profile,
                   AppListControllerDelegate* controller,
                   const std::string& app_id,
                   const std::u16string& window_title,
                   const std::u16string& heading_text,
                   const std::u16string& subheading_text,
                   const std::u16string& confirm_button_text,
                   const std::u16string& cancel_button_text,
                   ArcAppConfirmCallback confirm_callback);
  ArcAppDialogView(const ArcAppDialogView&) = delete;
  ArcAppDialogView& operator=(const ArcAppDialogView&) = delete;
  ~ArcAppDialogView() override;

  // Public method used for test only.
  void ConfirmOrCancelForTest(bool confirm);

 private:
  // AppIconLoaderDelegate:
  void OnAppImageUpdated(
      const std::string& app_id,
      const gfx::ImageSkia& image,
      bool is_placeholder_icon,
      const std::optional<gfx::ImageSkia>& badge_image) override;

  void AddMultiLineLabel(views::View* parent, const std::u16string& label_text);

  void OnDialogAccepted();
  void OnDialogCancelled();

  raw_ptr<views::ImageView> icon_view_ = nullptr;

  std::unique_ptr<AppServiceAppIconLoader> icon_loader_;

  const raw_ptr<Profile> profile_;

  const std::string app_id_;
  ArcAppConfirmCallback confirm_callback_;
};

// Browsertest use only. Global pointer of currently shown ArcAppDialogView.
ArcAppDialogView* g_current_arc_app_dialog_view = nullptr;

ArcAppDialogView::ArcAppDialogView(Profile* profile,
                                   AppListControllerDelegate* controller,
                                   const std::string& app_id,
                                   const std::u16string& window_title,
                                   const std::u16string& heading_text,
                                   const std::u16string& subheading_text,
                                   const std::u16string& confirm_button_text,
                                   const std::u16string& cancel_button_text,
                                   ArcAppConfirmCallback confirm_callback)
    : profile_(profile),
      app_id_(app_id),
      confirm_callback_(std::move(confirm_callback)) {
  SetTitle(window_title);
  SetButtonLabel(ui::mojom::DialogButton::kOk, confirm_button_text);
  SetButtonLabel(ui::mojom::DialogButton::kCancel, cancel_button_text);
  SetAcceptCallback(base::BindOnce(&ArcAppDialogView::OnDialogAccepted,
                                   base::Unretained(this)));
  SetCancelCallback(base::BindOnce(&ArcAppDialogView::OnDialogCancelled,
                                   base::Unretained(this)));

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      provider->GetDialogInsetsForContentType(views::DialogContentType::kText,
                                              views::DialogContentType::kText),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_HORIZONTAL)));

  SetModalType(ui::mojom::ModalType::kWindow);
  SetShowCloseButton(false);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));

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
  if (!subheading_text.empty()) {
    AddMultiLineLabel(text_container_ptr, subheading_text);
  }

  // The icon should be loaded asynchronously.
  icon_loader_ = std::make_unique<AppServiceAppIconLoader>(
      profile_, kIconSourceSize, this);
  icon_loader_->FetchImage(app_id_);

  g_current_arc_app_dialog_view = this;
  gfx::NativeWindow parent =
      controller ? controller->GetAppListWindow() : nullptr;
  constrained_window::CreateBrowserModalDialogViews(this, parent)->Show();
}

ArcAppDialogView::~ArcAppDialogView() {
  if (g_current_arc_app_dialog_view == this) {
    g_current_arc_app_dialog_view = nullptr;
  }
}

void ArcAppDialogView::AddMultiLineLabel(views::View* parent,
                                         const std::u16string& label_text) {
  auto label = std::make_unique<views::Label>(label_text);
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetAllowCharacterBreak(true);
  parent->AddChildView(std::move(label));
}

void ArcAppDialogView::ConfirmOrCancelForTest(bool confirm) {
  if (confirm) {
    AcceptDialog();
  } else {
    CancelDialog();
  }
}

void ArcAppDialogView::OnDialogAccepted() {
  // The dialog can either be accepted or cancelled, but never both.
  DCHECK(confirm_callback_);
  std::move(confirm_callback_).Run(true);
}

void ArcAppDialogView::OnDialogCancelled() {
  // The dialog can either be accepted or cancelled, but never both.
  DCHECK(confirm_callback_);
  std::move(confirm_callback_).Run(false);
}

void ArcAppDialogView::OnAppImageUpdated(
    const std::string& app_id,
    const gfx::ImageSkia& image,
    bool is_placeholder_icon,
    const std::optional<gfx::ImageSkia>& badge_image) {
  DCHECK_EQ(app_id, app_id_);
  DCHECK(!image.isNull());
  DCHECK_EQ(image.width(), kIconSourceSize);
  DCHECK_EQ(image.height(), kIconSourceSize);
  icon_view_->SetImageSize(image.size());
  icon_view_->SetImage(image);
}

BEGIN_METADATA(ArcAppDialogView)
END_METADATA

std::unique_ptr<ArcAppListPrefs::AppInfo> GetArcAppInfo(
    Profile* profile,
    const std::string& app_id) {
  ArcAppListPrefs* arc_prefs = ArcAppListPrefs::Get(profile);
  DCHECK(arc_prefs);
  return arc_prefs->GetApp(app_id);
}

}  // namespace

void ShowUsbScanDeviceListPermissionDialog(Profile* profile,
                                           const std::string& app_id,
                                           ArcUsbConfirmCallback callback) {
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      GetArcAppInfo(profile, app_id);
  if (!app_info) {
    std::move(callback).Run(false);
    return;
  }

  std::u16string window_title =
      l10n_util::GetStringUTF16(IDS_ARC_USB_PERMISSION_TITLE);

  std::u16string heading_text = l10n_util::GetStringFUTF16(
      IDS_ARC_USB_SCAN_DEVICE_LIST_PERMISSION_HEADING,
      base::UTF8ToUTF16(app_info->name));

  std::u16string confirm_button_text = l10n_util::GetStringUTF16(IDS_OK);

  std::u16string cancel_button_text = l10n_util::GetStringUTF16(IDS_CANCEL);

  new ArcAppDialogView(profile, nullptr /* controller */, app_id, window_title,
                       heading_text, std::u16string() /*subheading_text*/,
                       confirm_button_text, cancel_button_text,
                       std::move(callback));
}

void ShowUsbAccessPermissionDialog(Profile* profile,
                                   const std::string& app_id,
                                   const std::u16string& device_name,
                                   ArcUsbConfirmCallback callback) {
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      GetArcAppInfo(profile, app_id);
  if (!app_info) {
    std::move(callback).Run(false);
    return;
  }

  std::u16string window_title =
      l10n_util::GetStringUTF16(IDS_ARC_USB_PERMISSION_TITLE);

  std::u16string heading_text = l10n_util::GetStringFUTF16(
      IDS_ARC_USB_ACCESS_PERMISSION_HEADING, base::UTF8ToUTF16(app_info->name));

  std::u16string subheading_text = device_name;

  std::u16string confirm_button_text = l10n_util::GetStringUTF16(IDS_OK);

  std::u16string cancel_button_text = l10n_util::GetStringUTF16(IDS_CANCEL);

  new ArcAppDialogView(profile, nullptr /* controller */, app_id, window_title,
                       heading_text, subheading_text, confirm_button_text,
                       cancel_button_text, std::move(callback));
}

bool IsArcAppDialogViewAliveForTest() {
  return g_current_arc_app_dialog_view != nullptr;
}

bool CloseAppDialogViewAndConfirmForTest(bool confirm) {
  if (!g_current_arc_app_dialog_view) {
    return false;
  }

  g_current_arc_app_dialog_view->ConfirmOrCancelForTest(confirm);
  return true;
}

}  // namespace arc
