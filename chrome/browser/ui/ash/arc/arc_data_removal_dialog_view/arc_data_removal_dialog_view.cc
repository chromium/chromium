// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/app/arc_app_constants.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/app_list/app_service/app_service_app_icon_loader.h"
#include "chrome/browser/ash/app_list/arc/arc_data_removal_dialog.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/session/arc_session_manager_observer.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace arc {

namespace {

constexpr int kArcAppIconSize = 48;

// This dialog is shown when ARC++ comes into the state when normal
// functionality could not be possible without resetting whole container by data
// removal. It provides an option for the user to remove data and restart the
// ARC++ or keep current data. Following is known case:
//  * Child user failed to transit from/to regular state.
class DataRemovalConfirmationDialog : public views::DialogDelegateView,
                                      public AppIconLoaderDelegate,
                                      public ArcSessionManagerObserver {
  METADATA_HEADER(DataRemovalConfirmationDialog, views::DialogDelegateView)

 public:
  DataRemovalConfirmationDialog(
      Profile* profile,
      DataRemovalConfirmationCallback confirm_data_removal);
  DataRemovalConfirmationDialog(const DataRemovalConfirmationDialog&) = delete;
  DataRemovalConfirmationDialog& operator=(
      const DataRemovalConfirmationDialog&) = delete;
  ~DataRemovalConfirmationDialog() override;

  // AppIconLoaderDelegate:
  void OnAppImageUpdated(
      const std::string& app_id,
      const gfx::ImageSkia& image,
      bool is_placeholder_icon,
      const std::optional<gfx::ImageSkia>& badge_image) override;

  // ArcSessionManagerObserver:
  void OnArcPlayStoreEnabledChanged(bool enabled) override;

 private:
  // UI hierarchy owned.
  raw_ptr<views::ImageView> icon_view_ = nullptr;

  std::unique_ptr<AppServiceAppIconLoader> icon_loader_;

  const raw_ptr<Profile> profile_;

  DataRemovalConfirmationCallback confirm_callback_;
};

DataRemovalConfirmationDialog* g_current_data_removal_confirmation = nullptr;

DataRemovalConfirmationDialog::DataRemovalConfirmationDialog(
    Profile* profile,
    DataRemovalConfirmationCallback confirm_callback)
    : profile_(profile), confirm_callback_(std::move(confirm_callback)) {
  SetTitle(l10n_util::GetStringUTF16(IDS_ARC_DATA_REMOVAL_CONFIRMATION_TITLE));
  SetButtonLabel(
      ui::mojom::DialogButton::kOk,
      l10n_util::GetStringUTF16(IDS_ARC_DATA_REMOVAL_CONFIRMATION_OK_BUTTON));

  auto run_callback = [](DataRemovalConfirmationDialog* dialog, bool accept) {
    std::move(dialog->confirm_callback_).Run(accept);
  };
  SetAcceptCallback(base::BindOnce(run_callback, base::Unretained(this), true));
  SetCancelCallback(
      base::BindOnce(run_callback, base::Unretained(this), false));

  SetModalType(ui::mojom::ModalType::kWindow);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  std::unique_ptr<views::BoxLayout> layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      provider->GetDialogInsetsForContentType(views::DialogContentType::kText,
                                              views::DialogContentType::kText),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_HORIZONTAL));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  SetLayoutManager(std::move(layout));

  auto icon_view = std::make_unique<views::ImageView>();
  icon_view->SetPreferredSize(gfx::Size(kArcAppIconSize, kArcAppIconSize));
  icon_view_ = AddChildView(std::move(icon_view));

  // UI hierarchy owned.
  auto label = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_ARC_DATA_REMOVAL_CONFIRMATION_HEADING),
      views::style::CONTEXT_DIALOG_BODY_TEXT);
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(std::move(label));

  icon_loader_ = std::make_unique<AppServiceAppIconLoader>(
      profile_, kArcAppIconSize, this);
  icon_loader_->FetchImage(kPlayStoreAppId);

  ArcSessionManager::Get()->AddObserver(this);

  constrained_window::CreateBrowserModalDialogViews(this, nullptr)->Show();
}

DataRemovalConfirmationDialog::~DataRemovalConfirmationDialog() {
  ArcSessionManager::Get()->RemoveObserver(this);

  DCHECK_EQ(g_current_data_removal_confirmation, this);
  g_current_data_removal_confirmation = nullptr;
}

void DataRemovalConfirmationDialog::OnAppImageUpdated(
    const std::string& app_id,
    const gfx::ImageSkia& image,
    bool is_placeholder_icon,
    const std::optional<gfx::ImageSkia>& badge_image) {
  DCHECK(!image.isNull());
  DCHECK_EQ(image.width(), kArcAppIconSize);
  DCHECK_EQ(image.height(), kArcAppIconSize);
  icon_view_->SetImageSize(image.size());
  icon_view_->SetImage(image);
}

void DataRemovalConfirmationDialog::OnArcPlayStoreEnabledChanged(bool enabled) {
  // Close dialog on ARC++ OptOut. In this case data is automatically removed
  // and current dialog is no longer needed.
  if (enabled) {
    return;
  }
  CancelDialog();
}

BEGIN_METADATA(DataRemovalConfirmationDialog)
END_METADATA

}  // namespace

void ShowDataRemovalConfirmationDialog(
    Profile* profile,
    DataRemovalConfirmationCallback callback) {
  if (!g_current_data_removal_confirmation) {
    g_current_data_removal_confirmation =
        new DataRemovalConfirmationDialog(profile, std::move(callback));
  }
}

bool IsDataRemovalConfirmationDialogOpenForTesting() {
  return g_current_data_removal_confirmation != nullptr;
}

void CloseDataRemovalConfirmationDialogForTesting(bool confirm) {
  DCHECK(g_current_data_removal_confirmation);
  if (confirm) {
    g_current_data_removal_confirmation->AcceptDialog();
  } else {
    g_current_data_removal_confirmation->CancelDialog();
  }
}

}  // namespace arc
