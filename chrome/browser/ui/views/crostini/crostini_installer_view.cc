// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/crostini/crostini_installer_view.h"

#include <memory>
#include <vector>

#include "ash/public/cpp/ash_typography.h"
#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/system/sys_info.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/crostini/crostini_features.h"
#include "chrome/browser/chromeos/crostini/crostini_installer.h"
#include "chrome/browser/chromeos/crostini/crostini_installer_types.mojom.h"
#include "chrome/browser/chromeos/crostini/crostini_installer_ui_delegate.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/webui/chromeos/crostini_installer/crostini_installer_dialog.h"
#include "chrome/browser/ui/webui/chromeos/crostini_installer/crostini_installer_ui.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/window/dialog_client_view.h"

using crostini::CrostiniResult;
using crostini::mojom::InstallerError;
using crostini::mojom::InstallerState;

namespace {

CrostiniInstallerView* g_crostini_installer_view = nullptr;

constexpr gfx::Insets kOOBEButtonRowInsets(32, 64, 32, 64);
constexpr int kOOBEWindowWidth = 768;
// TODO(timloh): The button row's preferred height (48px) adds to this. I'm not
// sure where this actually comes from but since we plan to rewrite this dialog
// in WebUI soon, this constant just hard-coded here.
constexpr int kOOBEWindowHeight = 640 - 48;
constexpr int kLinuxIllustrationWidth = 448;
constexpr int kLinuxIllustrationHeight = 180;

constexpr char kCrostiniSetupSourceHistogram[] = "Crostini.SetupSource";

// Generates a Google Help URL which includes a "board type" parameter. Some
// help pages need to be adjusted depending on the type of CrOS device that is
// accessing the page.
base::string16 GetHelpUrlWithBoard(const std::string& original_url) {
  return base::ASCIIToUTF16(original_url +
                            "&b=" + base::SysInfo::GetLsbReleaseBoard());
}

base::string16 GetErrorMessage(InstallerError error) {
  switch (error) {
    case InstallerError::kErrorLoadingTermina:
      return l10n_util::GetStringUTF16(
          IDS_CROSTINI_INSTALLER_LOAD_TERMINA_ERROR);
    case InstallerError::kErrorStartingConcierge:
      return l10n_util::GetStringUTF16(
          IDS_CROSTINI_INSTALLER_START_CONCIERGE_ERROR);
    case InstallerError::kErrorCreatingDiskImage:
      return l10n_util::GetStringUTF16(
          IDS_CROSTINI_INSTALLER_CREATE_DISK_IMAGE_ERROR);
    case InstallerError::kErrorStartingTermina:
      return l10n_util::GetStringUTF16(
          IDS_CROSTINI_INSTALLER_START_TERMINA_VM_ERROR);
    case InstallerError::kErrorStartingContainer:
      return l10n_util::GetStringUTF16(
          IDS_CROSTINI_INSTALLER_START_CONTAINER_ERROR);
    case InstallerError::kErrorOffline:
      return l10n_util::GetStringFUTF16(IDS_CROSTINI_INSTALLER_OFFLINE_ERROR,
                                        ui::GetChromeOSDeviceName());
    case InstallerError::kErrorFetchingSshKeys:
      return l10n_util::GetStringUTF16(
          IDS_CROSTINI_INSTALLER_FETCH_SSH_KEYS_ERROR);
    case InstallerError::kErrorMountingContainer:
      return l10n_util::GetStringUTF16(
          IDS_CROSTINI_INSTALLER_MOUNT_CONTAINER_ERROR);
    case InstallerError::kErrorSettingUpContainer:
      return l10n_util::GetStringUTF16(
          IDS_CROSTINI_INSTALLER_SETUP_CONTAINER_ERROR);
    case InstallerError::kErrorInsufficientDiskSpace:
      return l10n_util::GetStringFUTF16(
          IDS_CROSTINI_INSTALLER_INSUFFICIENT_DISK,
          ui::FormatBytesWithUnits(
              crostini::CrostiniInstallerUIDelegate::kMinimumFreeDiskSpace,
              ui::DATA_UNITS_GIBIBYTE,
              /*show_units=*/true));
    default:
      return {};
  }
}

}  // namespace

// TODO(lxj): |CrostiniInstallerView| will be removed at some point. We will
// wait until then to find a better place for this function.
void crostini::ShowCrostiniInstallerView(
    Profile* profile,
    crostini::CrostiniUISurface ui_surface) {
  // Defensive check to prevent showing the installer when crostini is not
  // allowed.
  if (!CrostiniFeatures::Get()->IsUIAllowed(profile)) {
    return;
  }
  base::UmaHistogramEnumeration(kCrostiniSetupSourceHistogram, ui_surface,
                                crostini::CrostiniUISurface::kCount);

  if (chromeos::CrostiniInstallerUI::IsEnabled()) {
    return chromeos::CrostiniInstallerDialog::Show(profile);
  } else {
    return CrostiniInstallerView::Show(
        profile, crostini::CrostiniInstaller::GetForProfile(profile));
  }
}

// static
void CrostiniInstallerView::Show(
    Profile* profile,
    crostini::CrostiniInstallerUIDelegate* delegate) {
  DCHECK(crostini::CrostiniFeatures::Get()->IsUIAllowed(profile));
  if (!g_crostini_installer_view) {
    DCHECK(!crostini::CrostiniManager::GetForProfile(profile)
                ->GetInstallerViewStatus());
    g_crostini_installer_view = new CrostiniInstallerView(profile, delegate);
    views::DialogDelegate::CreateDialogWidget(g_crostini_installer_view,
                                              nullptr, nullptr);

    g_crostini_installer_view->GetDialogClientView()->SetButtonRowInsets(
        kOOBEButtonRowInsets);
    // We do our layout when the big message is at its biggest. Then we can
    // set it to the desired value.
    g_crostini_installer_view->big_message_label_->SetText(
        l10n_util::GetStringUTF16(IDS_CROSTINI_INSTALLER_INSTALLING));
    g_crostini_installer_view->GetWidget()->GetRootView()->Layout();
    const base::string16 device_type = ui::GetChromeOSDeviceName();
    g_crostini_installer_view->big_message_label_->SetText(
        l10n_util::GetStringFUTF16(IDS_CROSTINI_INSTALLER_TITLE, device_type));

    crostini::CrostiniManager::GetForProfile(profile)->SetInstallerViewStatus(
        true);
  }

  g_crostini_installer_view->GetWidget()->Show();
}

// static
CrostiniInstallerView* CrostiniInstallerView::GetActiveViewForTesting() {
  return g_crostini_installer_view;
}

int CrostiniInstallerView::GetDialogButtons() const {
  if (state_ == State::PROMPT || state_ == State::ERROR) {
    return ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL;
  }
  return ui::DIALOG_BUTTON_CANCEL;
}

base::string16 CrostiniInstallerView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_OK) {
    if (state_ == State::ERROR) {
      return l10n_util::GetStringUTF16(IDS_CROSTINI_INSTALLER_RETRY_BUTTON);
    }
    return l10n_util::GetStringUTF16(IDS_CROSTINI_INSTALLER_INSTALL_BUTTON);
  }
  DCHECK_EQ(button, ui::DIALOG_BUTTON_CANCEL);
  if (state_ == State::SUCCESS)
    return l10n_util::GetStringUTF16(IDS_APP_CLOSE);
  return l10n_util::GetStringUTF16(IDS_APP_CANCEL);
}

bool CrostiniInstallerView::IsDialogButtonEnabled(
    ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_CANCEL &&
      (state_ == State::CANCELING || state_ == State::CANCELED)) {
    return false;
  }
  return true;
}

bool CrostiniInstallerView::ShouldShowCloseButton() const {
  return false;
}

bool CrostiniInstallerView::ShouldShowWindowTitle() const {
  return false;
}

bool CrostiniInstallerView::Accept() {
  // This dialog can be accepted from State::ERROR. In that case, we're doing a
  // Retry.
  DCHECK(state_ == State::PROMPT || state_ == State::ERROR);

  // |learn_more_link_| should only be present in State::PROMPT.
  delete learn_more_link_;
  learn_more_link_ = nullptr;

  state_ = State::INSTALLING;
  installing_state_ = InstallerState::kStart;
  progress_bar_->SetValue(0);
  progress_bar_->SetVisible(true);
  SetMessageLabel();
  big_message_label_->SetText(
      l10n_util::GetStringUTF16(IDS_CROSTINI_INSTALLER_INSTALLING));
  DialogModelChanged();
  GetWidget()->GetRootView()->Layout();

  VLOG(1) << "delegate_->Install()";
  delegate_->Install(
      crostini::CrostiniManager::RestartOptions{},
      base::BindRepeating(&CrostiniInstallerView::OnProgressUpdate,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&CrostiniInstallerView::OnInstallFinished,
                     weak_ptr_factory_.GetWeakPtr()));

  return false;
}

bool CrostiniInstallerView::Cancel() {
  switch (state_) {
    case State::PROMPT:
      delegate_->CancelBeforeStart();
      return true;
    case State::INSTALLING:
      state_ = State::CANCELING;
      big_message_label_->SetText(
          l10n_util::GetStringUTF16(IDS_CROSTINI_INSTALLER_CANCELING_TITLE));
      message_label_->SetText(
          l10n_util::GetStringUTF16(IDS_CROSTINI_INSTALLER_CANCELING));
      message_label_->SetVisible(true);
      progress_bar_->SetValue(-1);
      progress_bar_->SetVisible(true);
      DialogModelChanged();
      GetWidget()->GetRootView()->Layout();
      delegate_->Cancel(base::BindOnce(&CrostiniInstallerView::OnCanceled,
                                       weak_ptr_factory_.GetWeakPtr()));
      return false;
    case State::CANCELING:
      return false;
    default:
      return true;  // Close the dialog.
  }
}

gfx::Size CrostiniInstallerView::CalculatePreferredSize() const {
  return gfx::Size(kOOBEWindowWidth, kOOBEWindowHeight);
}

void CrostiniInstallerView::LinkClicked(views::Link* source, int event_flags) {
  DCHECK_EQ(source, learn_more_link_);

  NavigateParams params(
      profile_, GURL(GetHelpUrlWithBoard(chrome::kLinuxAppsLearnMoreURL)),
      ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);
}

CrostiniInstallerView::CrostiniInstallerView(
    Profile* profile,
    crostini::CrostiniInstallerUIDelegate* delegate)
    : profile_(profile), delegate_(delegate) {
  // Layout constants from the spec.
  constexpr gfx::Insets kDialogInsets(60, 64, 0, 64);
  constexpr int kDialogSpacingVertical = 32;
  constexpr gfx::Size kLogoImageSize(32, 32);
  constexpr gfx::Insets kLowerContainerInsets(0, 0, 80, 0);

  views::BoxLayout* layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical, gfx::Insets(),
          kDialogSpacingVertical));

  views::View* upper_container_view = new views::View();
  upper_container_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, kDialogInsets,
      kDialogSpacingVertical));
  AddChildView(upper_container_view);

  views::View* lower_container_view = new views::View();
  views::BoxLayout* lower_container_layout =
      lower_container_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical, kLowerContainerInsets));
  AddChildView(lower_container_view);

  logo_image_ = new views::ImageView();
  logo_image_->SetImageSize(kLogoImageSize);
  logo_image_->SetImage(
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_LOGO_CROSTINI_DEFAULT_32));
  logo_image_->SetHorizontalAlignment(views::ImageView::Alignment::kLeading);
  upper_container_view->AddChildView(logo_image_);

  const base::string16 device_type = ui::GetChromeOSDeviceName();

  big_message_label_ =
      new views::Label({}, ash::AshTextContext::CONTEXT_HEADLINE_OVERSIZED);
  big_message_label_->SetMultiLine(true);
  big_message_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  upper_container_view->AddChildView(big_message_label_);

  // TODO(timloh): Descenders in the message appear to be clipped, re-visit once
  // the UI has been fleshed out more.
  const base::string16 message = l10n_util::GetStringFUTF16(
      IDS_CROSTINI_INSTALLER_BODY,
      ui::FormatBytesWithUnits(
          crostini::CrostiniInstallerUIDelegate::kDownloadSizeInBytes,
          ui::DATA_UNITS_MEBIBYTE,
          /*show_units=*/true));

  // Make a view to keep |message_label_| and |learn_more_link_| together with
  // less vertical spacing than the other dialog views.
  views::View* message_view = new views::View();
  message_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  message_label_ = new views::Label(message);
  message_label_->SetMultiLine(true);
  message_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  message_view->AddChildView(message_label_);

  learn_more_link_ = new views::Link(l10n_util::GetStringUTF16(IDS_LEARN_MORE));
  learn_more_link_->set_listener(this);
  learn_more_link_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  message_view->AddChildView(learn_more_link_);

  upper_container_view->AddChildView(message_view);

  // Make a slot for the progress bar, but it's not initially visible.
  progress_bar_ = new views::ProgressBar();
  upper_container_view->AddChildView(progress_bar_);
  progress_bar_->SetVisible(false);

  big_image_ = new views::ImageView();
  big_image_->SetImageSize(
      gfx::Size(kLinuxIllustrationWidth, kLinuxIllustrationHeight));
  big_image_->SetImage(
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_LINUX_ILLUSTRATION));
  lower_container_view->AddChildView(big_image_);

  // Make sure the lower_container_view is pinned to the bottom of the dialog.
  lower_container_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kEnd);
  layout->SetFlexForView(lower_container_view, 1, true);

  chrome::RecordDialogCreation(chrome::DialogIdentifier::CROSTINI_INSTALLER);
}

CrostiniInstallerView::~CrostiniInstallerView() {
  crostini::CrostiniManager::GetForProfile(profile_)->SetInstallerViewStatus(
      false);
  g_crostini_installer_view = nullptr;
}

void CrostiniInstallerView::OnProgressUpdate(InstallerState installing_state,
                                             double progress_fraction) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(state_, State::INSTALLING);
  if (installing_state_ != installing_state) {
    installing_state_ = installing_state;
    SetMessageLabel();
  }
  // |progress_bar_| has been set visible in |Accept()|.
  progress_bar_->SetValue(progress_fraction);

  DialogModelChanged();
  GetWidget()->GetRootView()->Layout();
}

void CrostiniInstallerView::OnInstallFinished(InstallerError error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (error == InstallerError::kNone) {
    state_ = State::SUCCESS;
    GetWidget()->Close();
    return;
  }

  state_ = State::ERROR;
  big_message_label_->SetText(
      l10n_util::GetStringUTF16(IDS_CROSTINI_INSTALLER_ERROR_TITLE));
  message_label_->SetText(GetErrorMessage(error));
  message_label_->SetVisible(true);
  progress_bar_->SetVisible(false);
  // Remove the buttons so they get recreated with correct color and
  // highlighting. Without this it is possible for both buttons to be styled
  // as default buttons.
  delete GetDialogClientView()->ok_button();
  delete GetDialogClientView()->cancel_button();

  DialogModelChanged();
  GetWidget()->GetRootView()->Layout();
}

void CrostiniInstallerView::OnCanceled() {
  state_ = State::CANCELED;
  GetWidget()->Close();
}

void CrostiniInstallerView::SetMessageLabel() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  int message_id = 0;

  switch (installing_state_) {
    case InstallerState::kInstallImageLoader:
      message_id = IDS_CROSTINI_INSTALLER_LOAD_TERMINA_MESSAGE;
      break;
    case InstallerState::kStartConcierge:
      message_id = IDS_CROSTINI_INSTALLER_START_CONCIERGE_MESSAGE;
      break;
    case InstallerState::kCreateDiskImage:
      message_id = IDS_CROSTINI_INSTALLER_CREATE_DISK_IMAGE_MESSAGE;
      break;
    case InstallerState::kStartTerminaVm:
      message_id = IDS_CROSTINI_INSTALLER_START_TERMINA_VM_MESSAGE;
      break;
    case InstallerState::kCreateContainer:
      // TODO(lxj): we are using the same message as for |START_CONTAINER|,
      // which is weird because user is going to see message "start container"
      // then "setup container" and then "start container" again.
      message_id = IDS_CROSTINI_INSTALLER_START_CONTAINER_MESSAGE;
      break;
    case InstallerState::kSetupContainer:
      message_id = IDS_CROSTINI_INSTALLER_SETUP_CONTAINER_MESSAGE;
      break;
    case InstallerState::kStartContainer:
      message_id = IDS_CROSTINI_INSTALLER_START_CONTAINER_MESSAGE;
      break;
    case InstallerState::kFetchSshKeys:
      message_id = IDS_CROSTINI_INSTALLER_FETCH_SSH_KEYS_MESSAGE;
      break;
    case InstallerState::kMountContainer:
      message_id = IDS_CROSTINI_INSTALLER_MOUNT_CONTAINER_MESSAGE;
      break;
    default:
      break;
  }

  if (message_id == 0) {
    message_label_->SetVisible(false);
  } else {
    message_label_->SetText(l10n_util::GetStringUTF16(message_id));
    message_label_->SetVisible(true);
  }
}
