// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/arc/arc_data_removal_dialog.h"

#include "base/macros.h"
#include "chrome/browser/chromeos/arc/session/arc_session_manager.h"
#include "chrome/browser/ui/app_list/arc/arc_app_icon_loader.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
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
                                      public ArcSessionManager::Observer {
 public:
  DataRemovalConfirmationDialog(
      Profile* profile,
      DataRemovalConfirmationCallback confirm_data_removal);
  ~DataRemovalConfirmationDialog() override;

  // views::WidgetDelegate:
  base::string16 GetWindowTitle() const override;
  ui::ModalType GetModalType() const override;

  // views::DialogDelegate:
  bool Accept() override;
  bool Cancel() override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;

  // AppIconLoaderDelegate:
  void OnAppImageUpdated(const std::string& app_id,
                         const gfx::ImageSkia& image) override;

  // ArcSessionManager::Observer:
  void OnArcPlayStoreEnabledChanged(bool enabled) override;

 private:
  // UI hierarchy owned.
  views::ImageView* icon_view_ = nullptr;

  std::unique_ptr<ArcAppIconLoader> icon_loader_;

  Profile* const profile_;

  DataRemovalConfirmationCallback confirm_callback_;

  DISALLOW_COPY_AND_ASSIGN(DataRemovalConfirmationDialog);
};

DataRemovalConfirmationDialog* g_current_data_removal_confirmation = nullptr;

DataRemovalConfirmationDialog::DataRemovalConfirmationDialog(
    Profile* profile,
    DataRemovalConfirmationCallback confirm_callback)
    : profile_(profile), confirm_callback_(std::move(confirm_callback)) {
  DialogDelegate::set_button_label(
      ui::DIALOG_BUTTON_OK,
      l10n_util::GetStringUTF16(IDS_ARC_DATA_REMOVAL_CONFIRMATION_OK_BUTTON));

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  std::unique_ptr<views::BoxLayout> layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      provider->GetDialogInsetsForContentType(views::TEXT, views::TEXT),
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
      views::style::CONTEXT_MESSAGE_BOX_BODY_TEXT);
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(std::move(label));

  icon_loader_ =
      std::make_unique<ArcAppIconLoader>(profile_, kArcAppIconSize, this);
  icon_loader_->FetchImage(kPlayStoreAppId);
  chrome::RecordDialogCreation(
      chrome::DialogIdentifier::ARC_DATA_REMOVAL_CONFIRMATION);

  ArcSessionManager::Get()->AddObserver(this);

  constrained_window::CreateBrowserModalDialogViews(this, nullptr)->Show();
}

DataRemovalConfirmationDialog::~DataRemovalConfirmationDialog() {
  ArcSessionManager::Get()->RemoveObserver(this);

  DCHECK_EQ(g_current_data_removal_confirmation, this);
  g_current_data_removal_confirmation = nullptr;
}

base::string16 DataRemovalConfirmationDialog::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_ARC_DATA_REMOVAL_CONFIRMATION_TITLE);
}

ui::ModalType DataRemovalConfirmationDialog::GetModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

bool DataRemovalConfirmationDialog::Accept() {
  if (confirm_callback_)
    std::move(confirm_callback_).Run(true);
  return true;
}

bool DataRemovalConfirmationDialog::Cancel() {
  if (confirm_callback_)
    std::move(confirm_callback_).Run(false);
  return true;
}

gfx::Size DataRemovalConfirmationDialog::CalculatePreferredSize() const {
  const int default_width = views::LayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH);
  return gfx::Size(default_width, GetHeightForWidth(default_width));
}

void DataRemovalConfirmationDialog::OnAppImageUpdated(
    const std::string& app_id,
    const gfx::ImageSkia& image) {
  DCHECK(!image.isNull());
  DCHECK_EQ(image.width(), kArcAppIconSize);
  DCHECK_EQ(image.height(), kArcAppIconSize);
  icon_view_->SetImageSize(image.size());
  icon_view_->SetImage(image);
}

void DataRemovalConfirmationDialog::OnArcPlayStoreEnabledChanged(bool enabled) {
  // Close dialog on ARC++ OptOut. In this case data is automatically removed
  // and current dialog is no longer needed.
  if (enabled)
    return;
  Cancel();
  GetWidget()->Close();
}

}  // namespace

void ShowDataRemovalConfirmationDialog(
    Profile* profile,
    DataRemovalConfirmationCallback callback) {
  if (!g_current_data_removal_confirmation)
    g_current_data_removal_confirmation =
        new DataRemovalConfirmationDialog(profile, std::move(callback));
}

bool IsDataRemovalConfirmationDialogOpenForTesting() {
  return g_current_data_removal_confirmation != nullptr;
}

void CloseDataRemovalConfirmationDialogForTesting(bool confirm) {
  DCHECK(g_current_data_removal_confirmation);
  if (confirm)
    g_current_data_removal_confirmation->Accept();
  else
    g_current_data_removal_confirmation->Cancel();
  g_current_data_removal_confirmation->GetWidget()->Close();
}

}  // namespace arc
