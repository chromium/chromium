// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/borealis/borealis_installer_view.h"

#include <memory>

#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/window_properties.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/borealis/borealis_context_manager.h"
#include "chrome/browser/ash/borealis/borealis_installer.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/view_class_properties.h"

namespace {

BorealisInstallerView* g_borealis_installer_view = nullptr;

constexpr gfx::Insets kButtonRowInsets(0, 64, 32, 64);
constexpr int kWindowWidth = 768;
constexpr int kWindowHeight = 636;

}  // namespace

// Defined in chrome/browser/ash/borealis/borealis_util.h.
void borealis::ShowBorealisInstallerView(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!g_borealis_installer_view) {
    g_borealis_installer_view = new BorealisInstallerView(profile);
    views::DialogDelegate::CreateDialogWidget(g_borealis_installer_view,
                                              nullptr, nullptr);
    g_borealis_installer_view->GetWidget()->GetNativeWindow()->SetProperty(
        ash::kShelfIDKey, ash::ShelfID(borealis::kBorealisAppId).Serialize());
  }
  g_borealis_installer_view->SetButtonRowInsets(kButtonRowInsets);
  g_borealis_installer_view->GetWidget()->Show();
}

// We need a separate class so that we can alert screen readers appropriately
// when the text changes.
class BorealisInstallerView::TitleLabel : public views::Label {
 public:
  using Label::Label;

  METADATA_HEADER(TitleLabel);

  TitleLabel() {}
  ~TitleLabel() override {}

  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    node_data->SetName(GetText());
    node_data->role = ax::mojom::Role::kStatus;
  }
};

BEGIN_METADATA(BorealisInstallerView, TitleLabel, views::Label)
END_METADATA

// TODO(danielng):revisit UI elements when UX input is provided.
// Currently using the UI specs that the Plugin VM installer use.
BorealisInstallerView::BorealisInstallerView(Profile* profile)
    : app_name_(l10n_util::GetStringUTF16(IDS_BOREALIS_APP_NAME)),
      profile_(profile) {
  // Layout constants from the spec used for the plugin vm installer.
  gfx::Insets kDialogInsets(60, 64, 0, 64);
  const int kPrimaryMessageHeight = views::style::GetLineHeight(
      CONTEXT_HEADLINE, views::style::STYLE_PRIMARY);
  const int kSecondaryMessageHeight = views::style::GetLineHeight(
      views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_SECONDARY);
  const int kInstallationProgressMessageHeight = views::style::GetLineHeight(
      CONTEXT_DIALOG_BODY_TEXT_SMALL, views::style::STYLE_SECONDARY);
  constexpr int kProgressBarHeight = 5;
  constexpr int kProgressBarTopMargin = 32;

  SetDefaultButton(ui::DIALOG_BUTTON_OK);
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
      views::kMarginsKey, gfx::Insets(kPrimaryMessageHeight, 0, 0, 0));
  primary_message_label_->SetMultiLine(false);
  primary_message_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  upper_container_view->AddChildView(primary_message_label_);

  views::View* secondary_message_container_view =
      AddChildView(std::make_unique<views::View>());
  secondary_message_container_view->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical,
          gfx::Insets(kSecondaryMessageHeight, 0, 0, 0)));
  upper_container_view->AddChildView(secondary_message_container_view);
  secondary_message_label_ = new views::Label(
      GetSecondaryMessage(), views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_SECONDARY);
  secondary_message_label_->SetMultiLine(true);
  secondary_message_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  secondary_message_container_view->AddChildView(secondary_message_label_);

  progress_bar_ = new views::ProgressBar(kProgressBarHeight);
  progress_bar_->SetProperty(
      views::kMarginsKey,
      gfx::Insets(kProgressBarTopMargin - kProgressBarHeight, 0, 0, 0));
  upper_container_view->AddChildView(progress_bar_);

  installation_progress_message_label_ =
      new views::Label(std::u16string(), CONTEXT_DIALOG_BODY_TEXT_SMALL,
                       views::style::STYLE_SECONDARY);
  installation_progress_message_label_->SetEnabledColor(gfx::kGoogleGrey700);
  installation_progress_message_label_->SetProperty(
      views::kMarginsKey,
      gfx::Insets(kInstallationProgressMessageHeight, 0, 0, 0));
  installation_progress_message_label_->SetMultiLine(false);
  installation_progress_message_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  upper_container_view->AddChildView(installation_progress_message_label_);

  big_image_ = new views::ImageView();
  lower_container_view->AddChildView(big_image_);

  // Make sure the lower_container_view is pinned to the bottom of the dialog.
  lower_container_layout_->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kEnd);
  layout->SetFlexForView(lower_container_view, 1, true);
}

BorealisInstallerView::~BorealisInstallerView() {
  borealis::BorealisInstaller& installer =
      borealis::BorealisService::GetForProfile(profile_)->Installer();
  installer.RemoveObserver(this);
  if (state_ == State::kConfirmInstall || state_ == State::kInstalling) {
    installer.Cancel();
  }
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
    borealis::BorealisService::GetForProfile(profile_)
        ->ContextManager()
        .StartBorealis(base::DoNothing());
    return true;
  }

  DCHECK_EQ(state_, State::kError);
  // Retry button has been clicked to retry setting of Borealis environment
  // after error occurred.
  StartInstallation();
  return false;
}

bool BorealisInstallerView::Cancel() {
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
    borealis::BorealisInstallResult result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  switch (result) {
    using ResultEnum = borealis::BorealisInstallResult;
    case ResultEnum::kSuccess:
      DCHECK_EQ(installing_state_, InstallingState::kInstallingDlc);
      state_ = State::kCompleted;
      break;
    case ResultEnum::kCancelled:
      break;
    // At this point we know an error has occurred.
    default:
      state_ = State::kError;
      result_ = result;
      break;
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
      return l10n_util::GetStringFUTF16(
          IDS_BOREALIS_INSTALLER_CONFIRMATION_TITLE, app_name_);
    case State::kInstalling:
      return l10n_util::GetStringFUTF16(
          IDS_BOREALIS_INSTALLER_ENVIRONMENT_SETTING_TITLE, app_name_);
    case State::kCompleted:
      return l10n_util::GetStringUTF16(IDS_BOREALIS_INSTALLER_FINISHED_TITLE);
    case State::kError:
      DCHECK(result_);
      switch (*result_) {
        case borealis::BorealisInstallResult::kBorealisNotAllowed:
        case borealis::BorealisInstallResult::kDlcUnsupportedError:
          return l10n_util::GetStringFUTF16(
              IDS_BOREALIS_INSTALLER_NOT_ALLOWED_TITLE, app_name_);
        default:
          return l10n_util::GetStringUTF16(IDS_BOREALIS_INSTALLER_ERROR_TITLE);
      }
  }
}

std::u16string BorealisInstallerView::GetSecondaryMessage() const {
  switch (state_) {
    case State::kConfirmInstall:
      return l10n_util::GetStringUTF16(
          IDS_BOREALIS_INSTALLER_CONFIRMATION_MESSAGE);
    case State::kInstalling:
      return l10n_util::GetStringUTF16(
          IDS_BOREALIS_INSTALLER_IMPORTING_MESSAGE);
    case State::kCompleted:
      return l10n_util::GetStringFUTF16(IDS_BOREALIS_INSTALLER_IMPORTED_MESSAGE,
                                        app_name_);
    case State::kError:
      using ResultEnum = borealis::BorealisInstallResult;
      DCHECK(result_);
      switch (*result_) {
        default:
        case ResultEnum::kBorealisInstallInProgress:
          return l10n_util::GetStringFUTF16(
              IDS_BOREALIS_INSTALLER_IN_PROGRESS_ERROR_MESSAGE, app_name_);
        case ResultEnum::kBorealisNotAllowed:
        case ResultEnum::kDlcUnsupportedError:
          return l10n_util::GetStringFUTF16(
              IDS_BOREALIS_INSTALLER_NOT_ALLOWED_MESSAGE, app_name_,
              base::NumberToString16(
                  static_cast<std::underlying_type_t<ResultEnum>>(*result_)));
        case ResultEnum::kOffline:
          return l10n_util::GetStringUTF16(
              IDS_BOREALIS_INSTALLER_OFFLINE_MESSAGE);
        // DLC Failures.
        case ResultEnum::kDlcInternalError:
          return l10n_util::GetStringUTF16(
              IDS_BOREALIS_DLC_INTERNAL_FAILED_MESSAGE);
        case ResultEnum::kDlcBusyError:
          return l10n_util::GetStringFUTF16(
              IDS_BOREALIS_DLC_BUSY_FAILED_MESSAGE, app_name_);
        case ResultEnum::kDlcNeedRebootError:
          return l10n_util::GetStringFUTF16(
              IDS_BOREALIS_DLC_NEED_REBOOT_FAILED_MESSAGE, app_name_);
        case ResultEnum::kDlcNeedSpaceError:
          return l10n_util::GetStringUTF16(
              IDS_BOREALIS_INSUFFICIENT_DISK_SPACE_MESSAGE);
        case ResultEnum::kDlcNeedUpdateError:
          return l10n_util::GetStringUTF16(
              IDS_BOREALIS_DLC_NEED_UPDATE_FAILED_MESSAGE);
        case ResultEnum::kDlcUnknownError:
          return l10n_util::GetStringFUTF16(
              IDS_BOREALIS_GENERIC_ERROR_MESSAGE, app_name_,
              base::NumberToString16(
                  static_cast<std::underlying_type_t<ResultEnum>>(*result_)));
      }
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
    case State::kError:
      DCHECK(result_);
      switch (*result_) {
        case borealis::BorealisInstallResult::kBorealisNotAllowed:
        case borealis::BorealisInstallResult::kDlcUnsupportedError:
        case borealis::BorealisInstallResult::kDlcNeedUpdateError:
          return ui::DIALOG_BUTTON_CANCEL;
        case borealis::BorealisInstallResult::kBorealisInstallInProgress:
        case borealis::BorealisInstallResult::kDlcInternalError:
        case borealis::BorealisInstallResult::kDlcBusyError:
        case borealis::BorealisInstallResult::kDlcNeedRebootError:
        case borealis::BorealisInstallResult::kDlcNeedSpaceError:
        case borealis::BorealisInstallResult::kDlcUnknownError:
        case borealis::BorealisInstallResult::kOffline:
          return ui::DIALOG_BUTTON_CANCEL | ui::DIALOG_BUTTON_OK;
        case borealis::BorealisInstallResult::kSuccess:
        case borealis::BorealisInstallResult::kCancelled:
          NOTREACHED();
          return 0;
      }
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
    case State::kError: {
      DCHECK(result_);
      switch (*result_) {
        case borealis::BorealisInstallResult::kBorealisNotAllowed:
          DCHECK_EQ(button, ui::DIALOG_BUTTON_CANCEL);
          return l10n_util::GetStringUTF16(IDS_APP_CANCEL);
        default:
          return l10n_util::GetStringUTF16(
              button == ui::DIALOG_BUTTON_OK
                  ? IDS_BOREALIS_INSTALLER_RETRY_BUTTON
                  : IDS_APP_CANCEL);
      }
    }
  }
}

void BorealisInstallerView::OnStateUpdated() {
  SetPrimaryMessageLabel();
  SetSecondaryMessageLabel();
  SetImage();

  // todo(danielng): ensure button labels meet a11y requirements.
  int buttons = GetCurrentDialogButtons();
  SetButtons(buttons);
  if (buttons & ui::DIALOG_BUTTON_OK) {
    SetButtonLabel(ui::DIALOG_BUTTON_OK,
                   GetCurrentDialogButtonLabel(ui::DIALOG_BUTTON_OK));
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

void BorealisInstallerView::SetImage() {
  constexpr gfx::Size kRegularImageSize(314, 191);
  constexpr gfx::Size kErrorImageSize(264, 264);
  constexpr int kRegularImageBottomInset = 52 + 57;
  constexpr int kErrorImageBottomInset = 52;

  auto setImage = [this](int image_id, gfx::Size size, int bottom_inset) {
    big_image_->SetImageSize(size);
    lower_container_layout_->set_inside_border_insets(
        gfx::Insets(0, 0, bottom_inset, 0));
    big_image_->SetImage(
        ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(image_id));
  };

  // todo(danielng):Use Borealis images.
  if (state_ == State::kError) {
    setImage(IDR_PLUGIN_VM_INSTALLER_ERROR, kErrorImageSize,
             kErrorImageBottomInset);
    return;
  }
  setImage(IDR_PLUGIN_VM_INSTALLER, kRegularImageSize,
           kRegularImageBottomInset);
}

void BorealisInstallerView::StartInstallation() {
  state_ = State::kInstalling;
  progress_bar_->SetValue(0);
  OnStateUpdated();

  borealis::BorealisInstaller& installer =
      borealis::BorealisService::GetForProfile(profile_)->Installer();
  installer.AddObserver(this);
  installer.Start();
}

BEGIN_METADATA(BorealisInstallerView, views::DialogDelegateView)
ADD_READONLY_PROPERTY_METADATA(std::u16string, PrimaryMessage)
ADD_READONLY_PROPERTY_METADATA(std::u16string, SecondaryMessage)
ADD_READONLY_PROPERTY_METADATA(int, CurrentDialogButtons)
END_METADATA
