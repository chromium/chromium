// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/borealis/borealis_installer_view.h"

#include <memory>

#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/style/color_mode_observer.h"
#include "ash/public/cpp/style/dark_light_mode_controller.h"
#include "ash/public/cpp/window_properties.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/borealis/borealis_app_launcher.h"
#include "chrome/browser/ash/borealis/borealis_context_manager.h"
#include "chrome/browser/ash/borealis/borealis_features.h"
#include "chrome/browser/ash/borealis/borealis_installer.h"
#include "chrome/browser/ash/borealis/borealis_metrics.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/views/borealis/borealis_installer_disallowed_dialog.h"
#include "chrome/browser/ui/views/borealis/borealis_installer_error_dialog.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

namespace {

BorealisInstallerView* g_borealis_installer_view = nullptr;

constexpr auto kButtonRowInsets = gfx::Insets::TLBR(0, 64, 32, 64);
constexpr int kWindowWidth = 768;
constexpr int kWindowHeight = 636;

void ShowBorealisInstallerViewIfAllowed(
    Profile* profile,
    borealis::BorealisFeatures::AllowStatus status) {
  if (status != borealis::BorealisFeatures::AllowStatus::kAllowed) {
    views::borealis::ShowInstallerDisallowedDialog(status);
    return;
  }

  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!g_borealis_installer_view) {
    g_borealis_installer_view = new BorealisInstallerView(profile);
    views::DialogDelegate::CreateDialogWidget(g_borealis_installer_view,
                                              nullptr, nullptr);
    g_borealis_installer_view->GetWidget()->GetNativeWindow()->SetProperty(
        ash::kShelfIDKey, ash::ShelfID(borealis::kInstallerAppId).Serialize());
  }
  g_borealis_installer_view->SetButtonRowInsets(kButtonRowInsets);

  g_borealis_installer_view->GetWidget()->Show();
}

}  // namespace

// Defined in chrome/browser/ash/borealis/borealis_util.h.
void borealis::ShowBorealisInstallerView(Profile* profile) {
  borealis::BorealisService::GetForProfile(profile)->Features().IsAllowed(
      base::BindOnce(&ShowBorealisInstallerViewIfAllowed, profile));
}

// We need a separate class so that we can alert screen readers appropriately
// when the text changes.
class BorealisInstallerView::TitleLabel : public views::Label {
 public:
  using Label::Label;

  METADATA_HEADER(TitleLabel);

  TitleLabel() = default;
  ~TitleLabel() override = default;

  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    node_data->role = ax::mojom::Role::kStatus;
    node_data->SetNameChecked(GetText());
  }
};

BEGIN_METADATA(BorealisInstallerView, TitleLabel, views::Label)
END_METADATA

BorealisInstallerView::BorealisInstallerView(Profile* profile)
    : app_name_(l10n_util::GetStringUTF16(IDS_BOREALIS_APP_NAME)),
      profile_(profile),
      observation_(this) {
  // Layout constants from the spec used for the plugin vm installer.
  constexpr auto kDialogInsets = gfx::Insets::TLBR(60, 64, 0, 64);
  const int kPrimaryMessageHeight = views::style::GetLineHeight(
      CONTEXT_HEADLINE, views::style::STYLE_PRIMARY);
  const int kSecondaryMessageHeight = views::style::GetLineHeight(
      views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_SECONDARY);
  const int kInstallationProgressMessageHeight = views::style::GetLineHeight(
      CONTEXT_DIALOG_BODY_TEXT_SMALL, views::style::STYLE_SECONDARY);
  constexpr int kProgressBarHeight = 5;
  constexpr int kProgressBarTopMargin = 32;

  SetCanMinimize(true);
  set_draggable(true);
  // Removed margins so dialog insets specify it instead.
  set_margins(gfx::Insets());

  views::BoxLayout* layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical, kDialogInsets));

  views::View* upper_container_view =
      AddChildView(std::make_unique<views::View>());
  upper_container_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets()));
  AddChildView(upper_container_view);

  views::View* lower_container_view =
      AddChildView(std::make_unique<views::View>());
  lower_container_layout_ =
      lower_container_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  AddChildView(lower_container_view);

  primary_message_label_ = new TitleLabel(GetPrimaryMessage(), CONTEXT_HEADLINE,
                                          views::style::STYLE_PRIMARY);
  primary_message_label_->SetProperty(
      views::kMarginsKey, gfx::Insets::TLBR(kPrimaryMessageHeight, 0, 0, 0));
  primary_message_label_->SetMultiLine(false);
  primary_message_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  upper_container_view->AddChildView(primary_message_label_.get());

  views::View* secondary_message_container_view =
      AddChildView(std::make_unique<views::View>());
  secondary_message_container_view->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical,
          gfx::Insets::TLBR(kSecondaryMessageHeight, 0, 0, 0)));
  upper_container_view->AddChildView(secondary_message_container_view);
  secondary_message_label_ = new views::Label(
      GetSecondaryMessage(), views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_SECONDARY);
  secondary_message_label_->SetMultiLine(true);
  secondary_message_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  secondary_message_container_view->AddChildView(
      secondary_message_label_.get());

  progress_bar_ = new views::ProgressBar(kProgressBarHeight);
  progress_bar_->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(kProgressBarTopMargin - kProgressBarHeight, 0, 0, 0));
  upper_container_view->AddChildView(progress_bar_.get());

  installation_progress_message_label_ =
      new views::Label(std::u16string(), CONTEXT_DIALOG_BODY_TEXT_SMALL,
                       views::style::STYLE_SECONDARY);
  installation_progress_message_label_->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(kInstallationProgressMessageHeight, 0, 0, 0));
  installation_progress_message_label_->SetMultiLine(false);
  installation_progress_message_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  upper_container_view->AddChildView(
      installation_progress_message_label_.get());

  big_image_ = new views::ImageView();
  lower_container_view->AddChildView(big_image_.get());

  // Make sure the lower_container_view is pinned to the bottom of the dialog.
  lower_container_layout_->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kEnd);
  layout->SetFlexForView(lower_container_view, 1, true);

  ash::DarkLightModeController* dark_light_controller =
      ash::DarkLightModeController::Get();
  if (dark_light_controller)
    dark_light_controller->AddObserver(this);
}

BorealisInstallerView::~BorealisInstallerView() {
  borealis::BorealisInstaller& installer =
      borealis::BorealisService::GetForProfile(profile_)->Installer();
  if (state_ == State::kConfirmInstall || state_ == State::kInstalling) {
    installer.Cancel();
  }
  ash::DarkLightModeController* dark_light_controller =
      ash::DarkLightModeController::Get();
  if (dark_light_controller)
    dark_light_controller->RemoveObserver(this);

  g_borealis_installer_view = nullptr;
}

// static
BorealisInstallerView* BorealisInstallerView::GetActiveViewForTesting() {
  return g_borealis_installer_view;
}

bool BorealisInstallerView::ShouldShowCloseButton() const {
  return true;
}

bool BorealisInstallerView::ShouldShowWindowTitle() const {
  return false;
}

bool BorealisInstallerView::Accept() {
  if (state_ == State::kConfirmInstall) {
    StartInstallation();
    return false;
  }

  if (state_ == State::kCompleted) {
    // Launch button has been clicked.
    borealis::BorealisService::GetForProfile(profile_)->AppLauncher().Launch(
        borealis::kClientAppId,
        base::BindOnce([](borealis::BorealisAppLauncher::LaunchResult result) {
          if (result == borealis::BorealisAppLauncher::LaunchResult::kSuccess)
            return;
          LOG(ERROR) << "Failed to launch borealis after install: code="
                     << static_cast<int>(result);
        }));
    return true;
  }
  // Retry button has been clicked to retry setting of Borealis environment
  // after error occurred.
  StartInstallation();
  return false;
}

bool BorealisInstallerView::Cancel() {
  if (state_ == State::kCompleted) {
    borealis::BorealisService::GetForProfile(profile_)
        ->ContextManager()
        .ShutDownBorealis(
            base::BindOnce([](borealis::BorealisShutdownResult result) {
              if (result == borealis::BorealisShutdownResult::kSuccess)
                return;
              LOG(ERROR) << "Failed to shutdown borealis after install: code="
                         << static_cast<int>(result);
            }));
  }
  return true;
}

void BorealisInstallerView::OnStateUpdated(InstallingState new_state) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(state_, State::kInstalling);
  DCHECK_NE(new_state, InstallingState::kInactive);
  installing_state_ = new_state;
  OnStateUpdated();
}

void BorealisInstallerView::OnProgressUpdated(double fraction_complete) {
  progress_bar_->SetValue(fraction_complete);
}

void BorealisInstallerView::OnInstallationEnded(
    borealis::BorealisInstallResult result,
    const std::string& error_description) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (result == borealis::BorealisInstallResult::kSuccess) {
    state_ = State::kCompleted;
  } else if (result != borealis::BorealisInstallResult::kCancelled) {
    result_ = result;
    LOG(ERROR) << "Borealis Installation Error: " << error_description;
    views::borealis::ShowInstallerErrorDialog(
        GetWidget()->GetNativeView(), result,
        base::BindOnce(&BorealisInstallerView::OnErrorDialogDismissed,
                       weak_factory_.GetWeakPtr()));
  }
  installing_state_ = InstallingState::kInactive;
  OnStateUpdated();
}

gfx::Size BorealisInstallerView::CalculatePreferredSize() const {
  return gfx::Size(kWindowWidth, kWindowHeight);
}

std::u16string BorealisInstallerView::GetPrimaryMessage() const {
  switch (state_) {
    case State::kConfirmInstall:
      return l10n_util::GetStringUTF16(
          IDS_BOREALIS_INSTALLER_CONFIRMATION_TITLE);
    case State::kInstalling:
      return l10n_util::GetStringUTF16(IDS_BOREALIS_INSTALLER_ONGOING_TITLE);

    case State::kCompleted:
      return l10n_util::GetStringUTF16(IDS_BOREALIS_INSTALLER_FINISHED_TITLE);
  }
}

std::u16string BorealisInstallerView::GetSecondaryMessage() const {
  switch (state_) {
    case State::kConfirmInstall:
      return l10n_util::GetStringUTF16(
          IDS_BOREALIS_INSTALLER_CONFIRMATION_MESSAGE);
    case State::kInstalling:
      return l10n_util::GetStringUTF16(IDS_BOREALIS_INSTALLER_ONGOING_MESSAGE);

    case State::kCompleted:
      return l10n_util::GetStringUTF16(IDS_BOREALIS_INSTALLER_FINISHED_MESSAGE);
  }
}

std::u16string BorealisInstallerView::GetProgressMessage() const {
  if (state_ != State::kInstalling)
    return {};
  switch (installing_state_) {
    case InstallingState::kInactive:
    case InstallingState::kCheckingIfAllowed:
      return l10n_util::GetStringUTF16(IDS_BOREALIS_INSTALLER_ONGOING_INACTIVE);
    case InstallingState::kInstallingDlc:
      return l10n_util::GetStringUTF16(IDS_BOREALIS_INSTALLER_ONGOING_DLC);
    case InstallingState::kStartingUp:
    case InstallingState::kAwaitingApplications:
      return l10n_util::GetStringUTF16(IDS_BOREALIS_INSTALLER_ONGOING_DRYRUN);
  }
}

void BorealisInstallerView::SetInstallingStateForTesting(
    InstallingState new_state) {
  installing_state_ = new_state;
}

int BorealisInstallerView::GetCurrentDialogButtons() const {
  switch (state_) {
    case State::kInstalling:
      return ui::DIALOG_BUTTON_CANCEL;
    case State::kConfirmInstall:
    case State::kCompleted:
      return ui::DIALOG_BUTTON_CANCEL | ui::DIALOG_BUTTON_OK;
  }
}

std::u16string BorealisInstallerView::GetCurrentDialogButtonLabel(
    ui::DialogButton button) const {
  switch (state_) {
    case State::kConfirmInstall:
      return l10n_util::GetStringUTF16(
          button == ui::DIALOG_BUTTON_OK ? IDS_BOREALIS_INSTALLER_INSTALL_BUTTON
                                         : IDS_APP_CANCEL);
    case State::kInstalling:
      DCHECK_EQ(button, ui::DIALOG_BUTTON_CANCEL);
      return l10n_util::GetStringUTF16(IDS_APP_CANCEL);
    case State::kCompleted: {
      return l10n_util::GetStringUTF16(
          button == ui::DIALOG_BUTTON_OK ? IDS_BOREALIS_INSTALLER_LAUNCH_BUTTON
                                         : IDS_APP_CLOSE);
    }
  }
}

void BorealisInstallerView::OnErrorDialogDismissed(
    views::borealis::ErrorDialogChoice choice) {
  switch (choice) {
    case views::borealis::ErrorDialogChoice::kRetry:
      StartInstallation();
      return;
    case views::borealis::ErrorDialogChoice::kExit:
      GetWidget()->Close();
      return;
  }
}

void BorealisInstallerView::OnStateUpdated() {
  SetPrimaryMessageLabel();
  SetSecondaryMessageLabel();
  SetProgressMessageLabel();
  SetImage();

  // todo(danielng): ensure button labels meet a11y requirements.
  int buttons = GetCurrentDialogButtons();
  SetButtons(buttons);
  if (buttons & ui::DIALOG_BUTTON_OK) {
    SetButtonLabel(ui::DIALOG_BUTTON_OK,
                   GetCurrentDialogButtonLabel(ui::DIALOG_BUTTON_OK));
    SetDefaultButton(ui::DIALOG_BUTTON_OK);
  } else {
    SetDefaultButton(ui::DIALOG_BUTTON_NONE);
  }
  if (buttons & ui::DIALOG_BUTTON_CANCEL) {
    SetButtonLabel(ui::DIALOG_BUTTON_CANCEL,
                   GetCurrentDialogButtonLabel(ui::DIALOG_BUTTON_CANCEL));
  }

  const bool progress_bar_visible = state_ == State::kInstalling;
  progress_bar_->SetVisible(progress_bar_visible);

  DialogModelChanged();
  primary_message_label_->NotifyAccessibilityEvent(
      ax::mojom::Event::kLiveRegionChanged,
      /* send_native_event = */ true);
}

void BorealisInstallerView::AddedToWidget() {
  // At this point GetWidget() is guaranteed to return non-null.
  OnStateUpdated();
}

void BorealisInstallerView::OnColorModeChanged(bool dark_mode_enabled) {
  // We check dark-mode ourselves, so no need to propagate the param.
  OnStateUpdated();
}

void BorealisInstallerView::SetPrimaryMessageLabel() {
  primary_message_label_->SetText(GetPrimaryMessage());
  primary_message_label_->SetVisible(true);
  primary_message_label_->NotifyAccessibilityEvent(
      ax::mojom::Event::kTextChanged, true);
}

void BorealisInstallerView::SetSecondaryMessageLabel() {
  secondary_message_label_->SetText(GetSecondaryMessage());
  secondary_message_label_->SetVisible(true);
  secondary_message_label_->NotifyAccessibilityEvent(
      ax::mojom::Event::kTextChanged, true);
}

void BorealisInstallerView::SetProgressMessageLabel() {
  std::u16string message = GetProgressMessage();
  installation_progress_message_label_->SetText(message);
  installation_progress_message_label_->SetVisible(!message.empty());
  installation_progress_message_label_->NotifyAccessibilityEvent(
      ax::mojom::Event::kTextChanged, true);
}

void BorealisInstallerView::SetImage() {
  // These values are adjusted from the mocks in b/246659720, to account for
  // differences in image resolution.
  constexpr int kStartBottomInsetDp = 70;
  constexpr int kCompleteBottomInsetDp = 64;

  auto set_image = [this](int image_id, int bottom_inset_dp) {
    lower_container_layout_->set_inside_border_insets(
        gfx::Insets::TLBR(0, 0, bottom_inset_dp, 0));
    gfx::ImageSkia* s =
        ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(image_id);
    // The image assets are sized so that we can display them at half their
    // resolution in DP.
    big_image_->SetImageSize({s->width() / 2, s->height() / 2});
    big_image_->SetImage(s);
  };

  ash::DarkLightModeController* dark_light_mode_controller =
      ash::DarkLightModeController::Get();
  bool dark_mode = dark_light_mode_controller &&
                   dark_light_mode_controller->IsDarkModeEnabled();

  if (state_ == State::kCompleted) {
    set_image(dark_mode ? IDR_BOREALIS_INSTALLER_COMPLETE_DARK
                        : IDR_BOREALIS_INSTALLER_COMPLETE_LIGHT,
              kCompleteBottomInsetDp);
    return;
  }
  set_image(dark_mode ? IDR_BOREALIS_INSTALLER_START_DARK
                      : IDR_BOREALIS_INSTALLER_START_LIGHT,
            kStartBottomInsetDp);
}

void BorealisInstallerView::StartInstallation() {
  state_ = State::kInstalling;
  progress_bar_->SetValue(0);

  borealis::BorealisInstaller& installer =
      borealis::BorealisService::GetForProfile(profile_)->Installer();
  if (observation_.IsObserving())
    observation_.Reset();
  observation_.Observe(&installer);
  installer.Start();

  OnStateUpdated();
}

BEGIN_METADATA(BorealisInstallerView, views::DialogDelegateView)
ADD_READONLY_PROPERTY_METADATA(std::u16string, PrimaryMessage)
ADD_READONLY_PROPERTY_METADATA(std::u16string, SecondaryMessage)
ADD_READONLY_PROPERTY_METADATA(int, CurrentDialogButtons)
END_METADATA
