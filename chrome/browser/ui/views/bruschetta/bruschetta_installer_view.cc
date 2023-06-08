// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bruschetta/bruschetta_installer_view.h"

#include <memory>

#include "ash/public/cpp/style/color_mode_observer.h"
#include "ash/public/cpp/style/dark_light_mode_controller.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/bruschetta/bruschetta_installer.h"
#include "chrome/browser/ash/bruschetta/bruschetta_installer_impl.h"
#include "chrome/browser/ash/bruschetta/bruschetta_pref_names.h"
#include "chrome/browser/ash/bruschetta/bruschetta_service.h"
#include "chrome/browser/ash/bruschetta/bruschetta_util.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/button/radio_button.h"
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
  if (bruschetta::GetInstallableConfigs(profile).empty()) {
    LOG(ERROR)
        << "Bruschetta has no installable configs, not running the installer.";
    return;
  }
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

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, kDialogInsets));

  views::View* upper_container_view =
      AddChildView(std::make_unique<views::View>());
  upper_container_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets()));

  radio_button_container_ = AddChildView(std::make_unique<views::View>());
  radio_button_container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

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

  {
    std::vector<bruschetta::InstallableConfig> configs =
        bruschetta::GetInstallableConfigs(profile_).extract();
    bruschetta::SortInstallableConfigs(&configs);
    for (const auto& [config_name, config_dict] : configs) {
      const auto& label =
          config_dict.Find(bruschetta::prefs::kPolicyNameKey)->GetString();

      auto* radio_button = radio_button_container_->AddChildView(
          std::make_unique<views::RadioButton>(base::UTF8ToUTF16(label)));

      radio_buttons_.emplace(config_name, radio_button);
    }
  }

  DCHECK(radio_button_container_->children().size() > 0);
  static_cast<views::RadioButton*>(radio_button_container_->children()[0])
      ->SetChecked(true);

  ash::DarkLightModeController* dark_light_controller =
      ash::DarkLightModeController::Get();
  if (dark_light_controller) {
    dark_light_controller->AddObserver(this);
  }
  installer_factory_ =
      base::BindRepeating([](Profile* profile, base::OnceClosure closure) {
        return static_cast<std::unique_ptr<bruschetta::BruschettaInstaller>>(
            std::make_unique<bruschetta::BruschettaInstallerImpl>(
                profile, std::move(closure)));
      });
}

BruschettaInstallerView::~BruschettaInstallerView() {
  // installer_->Cancel calls back into us, so remember that we're being
  // destroyed now to avoid doing work (that crashes us) in the callback.
  is_destroying_ = true;
  if (installer_) {
    installer_->Cancel();
  }
  observation_.Reset();
  g_bruschetta_installer_view = nullptr;
}

bool BruschettaInstallerView::Accept() {
  DCHECK(state_ == State::kConfirmInstall || state_ == State::kFailed ||
         state_ == State::kFailedCleanup);

  if (state_ == State::kConfirmInstall) {
    absl::optional<std::string> selected_config;
    for (const auto& it : radio_buttons_) {
      if (it.second->GetChecked()) {
        selected_config = it.first;
      }
    }

    DCHECK(selected_config.has_value()) << "No install config selected";
    selected_config_ = *selected_config;

    RemoveChildViewT(radio_button_container_.get());
    radio_button_container_ = nullptr;
    radio_buttons_.clear();
  }

  observation_.Reset();
  installer_.reset();
  StartInstallation();
  return false;
}

bool BruschettaInstallerView::Cancel() {
  if (state_ == State::kInstalling) {
    CleanupPartialInstall();
  }
  // We're about to get destroyed, and since all the cleanup happens in our
  // destructor there's nothing special to do here.
  return true;
}

void BruschettaInstallerView::StartInstallation() {
  state_ = State::kInstalling;
  progress_bar_->SetValue(-1);

  DCHECK(!installer_)
      << "Expect to create a new installer every run, but already had one";
  installer_ = installer_factory_.Run(
      profile_, base::BindOnce(&BruschettaInstallerView::OnInstallationEnded,
                               weak_factory_.GetWeakPtr()));
  observation_.Observe(installer_.get());
  installer_->Install(guest_id_.vm_name, selected_config_);

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
  CleanupPartialInstall();
  OnStateUpdated();
}

void BruschettaInstallerView::OnInstallationEnded() {
  if (is_destroying_) {
    return;
  }
  observation_.Reset();
  installer_.reset();
  GetWidget()->CloseWithReason(views::Widget::ClosedReason::kUnspecified);

  if (finish_callback_) {
    std::move(finish_callback_)
        .Run(bruschetta::BruschettaInstallResult::kSuccess);
  }
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
    case State::kCleaningUp:
    case State::kFailed:
    case State::kFailedCleanup:
      return l10n_util::GetStringUTF16(IDS_BRUSCHETTA_INSTALLER_ERROR_TITLE);
  }
}

std::u16string BruschettaInstallerView::GetSecondaryMessage() const {
  switch (state_) {
    case State::kInstalling:
      switch (installing_state_) {
        case InstallerState::kInstallStarted:
          // We don't really spend any time in the InstallStarted state, the
          // real first step is installing DLC so fall through to that.
        case InstallerState::kToolsDlcInstall:
        case InstallerState::kFirmwareDlcInstall:
          return l10n_util::GetStringUTF16(
              IDS_BRUSCHETTA_INSTALLER_INSTALLING_DLC_MESSAGE);
        case InstallerState::kBootDiskDownload:
        case InstallerState::kPflashDownload:
        case InstallerState::kOpenFiles:
          return l10n_util::GetStringUTF16(
              IDS_BRUSCHETTA_INSTALLER_DOWNLOADING_MESSAGE);
        case InstallerState::kCreateVmDisk:
        case InstallerState::kInstallPflash:
        case InstallerState::kStartVm:
        case InstallerState::kLaunchTerminal:
          return l10n_util::GetStringUTF16(
              IDS_BRUSCHETTA_INSTALLER_STARTING_VM_MESSAGE);
      }
    case State::kCleaningUp:
      return l10n_util::GetStringUTF16(
          IDS_BRUSCHETTA_INSTALLER_CLEANING_UP_MESSAGE);
    case State::kFailed:
      return l10n_util::GetStringFUTF16(
          IDS_BRUSCHETTA_INSTALLER_ERROR_MESSAGE,
          bruschetta::BruschettaInstallResultString(error_));
    case State::kFailedCleanup:
      return l10n_util::GetStringFUTF16(
          IDS_BRUSCHETTA_INSTALLER_ERROR_CLEANUP_MESSAGE,
          bruschetta::BruschettaInstallResultString(error_));
    case State::kConfirmInstall:
      return {};
  }
}

int BruschettaInstallerView::GetCurrentDialogButtons() const {
  switch (state_) {
    case State::kInstalling:
      return ui::DIALOG_BUTTON_CANCEL;
    case State::kConfirmInstall:
      // Cancel | Start installing
      return ui::DIALOG_BUTTON_CANCEL | ui::DIALOG_BUTTON_OK;
    case State::kCleaningUp:
      return 0;
    case State::kFailedCleanup:
    case State::kFailed:
      // Quit | Retry
      return ui::DIALOG_BUTTON_CANCEL | ui::DIALOG_BUTTON_OK;
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
    case State::kCleaningUp:
      return {};
    case State::kFailed:
    case State::kFailedCleanup:
      return l10n_util::GetStringUTF16(
          button == ui::DIALOG_BUTTON_OK ? IDS_BRUSCHETTA_INSTALLER_RETRY_BUTTON
                                         : IDS_APP_CLOSE);
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

  const bool progress_bar_visible =
      (state_ == State::kInstalling || state_ == State::kCleaningUp);
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

void BruschettaInstallerView::CleanupPartialInstall() {
  state_ = State::kCleaningUp;
  OnStateUpdated();
  bruschetta::BruschettaService::GetForProfile(profile_)->RemoveVm(
      guest_id_,
      base::BindOnce(&BruschettaInstallerView::UninstallBruschettaFinished,
                     weak_factory_.GetWeakPtr()));
}

void BruschettaInstallerView::UninstallBruschettaFinished(bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to clean up after a failed install";
    state_ = State::kFailedCleanup;
  } else {
    state_ = State::kFailed;
  }
  OnStateUpdated();

  if (finish_callback_) {
    std::move(finish_callback_).Run(error_);
  }
}

BEGIN_METADATA(BruschettaInstallerView, views::DialogDelegateView)
ADD_READONLY_PROPERTY_METADATA(std::u16string, PrimaryMessage)
ADD_READONLY_PROPERTY_METADATA(std::u16string, SecondaryMessage)
ADD_READONLY_PROPERTY_METADATA(int, CurrentDialogButtons)
END_METADATA
