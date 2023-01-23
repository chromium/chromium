// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bruschetta/bruschetta_installer_view.h"

#include <memory>

#include "ash/public/cpp/style/color_mode_observer.h"
#include "ash/public/cpp/style/dark_light_mode_controller.h"
#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/bruschetta/bruschetta_installer.h"
#include "chrome/browser/ash/bruschetta/bruschetta_installer_impl.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

namespace {

BruschettaInstallerView* g_bruschetta_installer_view = nullptr;

constexpr auto kButtonRowInsets = gfx::Insets::TLBR(0, 64, 32, 64);
constexpr int kWindowWidth = 768;
constexpr int kWindowHeight = 636;

}  // namespace

// static
void BruschettaInstallerView::Show(Profile* profile,
                                   const guest_os::GuestId& guest_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!g_bruschetta_installer_view) {
    g_bruschetta_installer_view =
        new BruschettaInstallerView(profile, guest_id);
    views::DialogDelegate::CreateDialogWidget(g_bruschetta_installer_view,
                                              nullptr, nullptr);
  }
  g_bruschetta_installer_view->SetButtonRowInsets(kButtonRowInsets);

  g_bruschetta_installer_view->GetWidget()->Show();
}

// static
BruschettaInstallerView* BruschettaInstallerView::GetActiveViewForTesting() {
  return g_bruschetta_installer_view;
}

// We need a separate class so that we can alert screen readers appropriately
// when the text changes.
class BruschettaInstallerView::TitleLabel : public views::Label {
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

BEGIN_METADATA(BruschettaInstallerView, TitleLabel, views::Label)
END_METADATA

BruschettaInstallerView::BruschettaInstallerView(Profile* profile,
                                                 guest_os::GuestId guest_id)
    : profile_(profile), observation_(this), guest_id_(guest_id) {
  // Layout constants from the spec used for the plugin vm installer.
  constexpr auto kDialogInsets = gfx::Insets::TLBR(60, 64, 0, 64);
  const int kPrimaryMessageHeight = views::style::GetLineHeight(
      CONTEXT_HEADLINE, views::style::STYLE_PRIMARY);
  const int kSecondaryMessageHeight = views::style::GetLineHeight(
      views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_SECONDARY);
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

  // Make sure the lower_container_view is pinned to the bottom of the dialog.
  lower_container_layout_->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kEnd);
  layout->SetFlexForView(lower_container_view, 1, true);

  ash::DarkLightModeController* dark_light_controller =
      ash::DarkLightModeController::Get();
  if (dark_light_controller) {
    dark_light_controller->AddObserver(this);
  }
}

BruschettaInstallerView::~BruschettaInstallerView() {
  // installer_->Cancel calls back into us, so remember that we're being
  // destroyed now to avoid doing work (that crashes us) in the callback.
  is_destroying_ = true;
  if (installer_) {
    installer_->Cancel();
  }
  g_bruschetta_installer_view = nullptr;
}

bool BruschettaInstallerView::Accept() {
  if (state_ == State::kConfirmInstall) {
    StartInstallation();
    return false;
  }

  // If we're not on the confirmation page then we're on the end page.
  return true;
}

bool BruschettaInstallerView::Cancel() {
  observation_.Reset();
  if (state_ == State::kInstalling) {
    installer_->Cancel();
  }
  return true;
}

void BruschettaInstallerView::StartInstallation() {
  state_ = State::kInstalling;
  progress_bar_->SetValue(-1);

  if (!installer_) {
    installer_ = std::make_unique<bruschetta::BruschettaInstallerImpl>(
        profile_, base::BindOnce(&BruschettaInstallerView::OnInstallationEnded,
                                 weak_factory_.GetWeakPtr()));
  } else {
    // Only test code should have an existing installer, non-test-code should
    // always be hitting the above branch to create an instance since we have a
    // singleton view and no way of going back to a previous page.
    CHECK_IS_TEST();
  }
  observation_.Observe(installer_.get());
  installer_->Install(guest_id_.vm_name, guest_id_.vm_name);

  OnStateUpdated();
}

void BruschettaInstallerView::StateChanged(InstallerState new_state) {
  VLOG(2) << "State changed: " << static_cast<int>(installing_state_) << " -> "
          << static_cast<int>(new_state);
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(state_, State::kInstalling);
  installing_state_ = new_state;
  OnStateUpdated();
}

void BruschettaInstallerView::Error(bruschetta::BruschettaInstallResult error) {
  error_ = error;
  state_ = State::kFailed;
  OnStateUpdated();
}

void BruschettaInstallerView::OnInstallationEnded() {
  if (is_destroying_) {
    return;
  }
  observation_.Reset();
  installer_.reset();
  GetWidget()->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
}

bool BruschettaInstallerView::ShouldShowCloseButton() const {
  return true;
}

bool BruschettaInstallerView::ShouldShowWindowTitle() const {
  return false;
}

gfx::Size BruschettaInstallerView::CalculatePreferredSize() const {
  return gfx::Size(kWindowWidth, kWindowHeight);
}

std::u16string BruschettaInstallerView::GetPrimaryMessage() const {
  switch (state_) {
    case State::kConfirmInstall:
      return l10n_util::GetStringUTF16(
          IDS_BRUSCHETTA_INSTALLER_CONFIRMATION_TITLE);
    case State::kInstalling:
      return l10n_util::GetStringUTF16(IDS_BRUSCHETTA_INSTALLER_ONGOING_TITLE);
    case State::kFailed:
      return l10n_util::GetStringUTF16(IDS_BRUSCHETTA_INSTALLER_ERROR_TITLE);
  }
}

std::u16string BruschettaInstallerView::GetSecondaryMessage() const {
  if (state_ == State::kInstalling) {
    switch (installing_state_) {
      case InstallerState::kInstallStarted:
        // We don't really spend any time in the InstallStarted state, the real
        // first step is installing DLC so fall through to that.
      case InstallerState::kDlcInstall:
        return l10n_util::GetStringUTF16(
            IDS_BRUSCHETTA_INSTALLER_INSTALLING_DLC_MESSAGE);
      case InstallerState::kBootDiskDownload:
      case InstallerState::kFirmwareDownload:
      case InstallerState::kPflashDownload:
      case InstallerState::kOpenFiles:
        return l10n_util::GetStringUTF16(
            IDS_BRUSCHETTA_INSTALLER_DOWNLOADING_MESSAGE);
      case InstallerState::kCreateVmDisk:
      case InstallerState::kStartVm:
      case InstallerState::kLaunchTerminal:
        return l10n_util::GetStringUTF16(
            IDS_BRUSCHETTA_INSTALLER_STARTING_VM_MESSAGE);
    }
  } else if (state_ == State::kFailed) {
    return l10n_util::GetStringFUTF16(
        IDS_BRUSCHETTA_INSTALLER_ERROR_MESSAGE,
        bruschetta::BruschettaInstallResultString(error_));
  }
  return {};
}

int BruschettaInstallerView::GetCurrentDialogButtons() const {
  switch (state_) {
    case State::kInstalling:
      return ui::DIALOG_BUTTON_CANCEL;
    case State::kConfirmInstall:
      return ui::DIALOG_BUTTON_CANCEL | ui::DIALOG_BUTTON_OK;
    case State::kFailed:
      return ui::DIALOG_BUTTON_CANCEL;
  }
}

std::u16string BruschettaInstallerView::GetCurrentDialogButtonLabel(
    ui::DialogButton button) const {
  switch (state_) {
    case State::kConfirmInstall:
      return l10n_util::GetStringUTF16(
          button == ui::DIALOG_BUTTON_OK
              ? IDS_BRUSCHETTA_INSTALLER_INSTALL_BUTTON
              : IDS_APP_CANCEL);
    case State::kInstalling:
      DCHECK_EQ(button, ui::DIALOG_BUTTON_CANCEL);
      return l10n_util::GetStringUTF16(IDS_APP_CANCEL);
    case State::kFailed:
      return l10n_util::GetStringUTF16(IDS_APP_CLOSE);
  }
}

void BruschettaInstallerView::OnStateUpdated() {
  SetPrimaryMessageLabel();
  SetSecondaryMessageLabel();

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

void BruschettaInstallerView::AddedToWidget() {
  // At this point GetWidget() is guaranteed to return non-null.
  OnStateUpdated();
}

void BruschettaInstallerView::OnColorModeChanged(bool dark_mode_enabled) {
  // We check dark-mode ourselves, so no need to propagate the param.
  OnStateUpdated();
}

void BruschettaInstallerView::SetPrimaryMessageLabel() {
  primary_message_label_->SetText(GetPrimaryMessage());
  primary_message_label_->SetVisible(true);
  primary_message_label_->NotifyAccessibilityEvent(
      ax::mojom::Event::kTextChanged, true);
}

void BruschettaInstallerView::SetSecondaryMessageLabel() {
  secondary_message_label_->SetText(GetSecondaryMessage());
  secondary_message_label_->SetVisible(true);
  secondary_message_label_->NotifyAccessibilityEvent(
      ax::mojom::Event::kTextChanged, true);
}

BEGIN_METADATA(BruschettaInstallerView, views::DialogDelegateView)
ADD_READONLY_PROPERTY_METADATA(std::u16string, PrimaryMessage)
ADD_READONLY_PROPERTY_METADATA(std::u16string, SecondaryMessage)
ADD_READONLY_PROPERTY_METADATA(int, CurrentDialogButtons)
END_METADATA
