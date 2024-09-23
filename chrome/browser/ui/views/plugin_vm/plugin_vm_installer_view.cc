// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/plugin_vm/plugin_vm_installer_view.h"

#include <memory>
#include <optional>

#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/window_properties.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_installer_factory.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_manager.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_manager_factory.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_thread.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

// This file contains VLOG logging to aid debugging tast tests.
#define LOG_FUNCTION_CALL() \
  VLOG(2) << "PluginVmInstallerView::" << __func__ << " called"

namespace {

PluginVmInstallerView* g_plugin_vm_installer_view = nullptr;

constexpr auto kButtonRowInsets = gfx::Insets::TLBR(0, 64, 32, 64);
constexpr int kWindowWidth = 768;
constexpr int kWindowHeight = 636;

// There's no point showing a retry button if it is guaranteed to still fail.
bool ShowRetryButton(plugin_vm::PluginVmInstaller::FailureReason reason) {
  switch (reason) {
    case plugin_vm::PluginVmInstaller::FailureReason::NOT_ALLOWED:
    case plugin_vm::PluginVmInstaller::FailureReason::DLC_INTERNAL:
    case plugin_vm::PluginVmInstaller::FailureReason::DLC_NEED_REBOOT:
    case plugin_vm::PluginVmInstaller::FailureReason::DLC_UNSUPPORTED:
      return false;
    default:
      return true;
  }
}

int HttpErrorFailureReasonToInt(
    plugin_vm::PluginVmInstaller::FailureReason reason) {
  using Reason = plugin_vm::PluginVmInstaller::FailureReason;
  switch (reason) {
    default:
      NOTREACHED();
    case Reason::DOWNLOAD_FAILED_401:
      return 401;
    case Reason::DOWNLOAD_FAILED_403:
      return 403;
    case Reason::DOWNLOAD_FAILED_404:
      return 404;
  }
}

}  // namespace

void plugin_vm::ShowPluginVmInstallerView(Profile* profile) {
  LOG_FUNCTION_CALL();
  if (!g_plugin_vm_installer_view) {
    g_plugin_vm_installer_view = new PluginVmInstallerView(profile);
    views::DialogDelegate::CreateDialogWidget(g_plugin_vm_installer_view,
                                              nullptr, nullptr);
    g_plugin_vm_installer_view->GetWidget()->GetNativeWindow()->SetProperty(
        ash::kShelfIDKey,
        ash::ShelfID(plugin_vm::kPluginVmShelfAppId).Serialize());
  }
  g_plugin_vm_installer_view->SetButtonRowInsets(kButtonRowInsets);
  g_plugin_vm_installer_view->GetWidget()->Show();
}

PluginVmInstallerView::PluginVmInstallerView(Profile* profile)
    : profile_(profile),
      app_name_(l10n_util::GetStringUTF16(IDS_PLUGIN_VM_APP_NAME)),
      plugin_vm_installer_(
          plugin_vm::PluginVmInstallerFactory::GetForProfile(profile)) {
  VLOG(2) << "PluginVmInstallerView created";
  // Layout constants from the spec.
  constexpr auto kDialogInsets = gfx::Insets::TLBR(60, 64, 0, 64);
  constexpr gfx::Size kLogoImageSize(32, 32);
  constexpr int kTitleFontSize = 28;
  const gfx::FontList kTitleFont({"Google Sans"}, gfx::Font::NORMAL,
                                 kTitleFontSize, gfx::Font::Weight::NORMAL);
  constexpr int kTitleHeight = 64;
  constexpr int kMessageFontSize = 13;
  const gfx::FontList kMessageFont({"Roboto"}, gfx::Font::NORMAL,
                                   kMessageFontSize, gfx::Font::Weight::NORMAL);
  constexpr int kMessageHeight = 32;
  constexpr int kDownloadProgressMessageFontSize = 12;
  const gfx::FontList kDownloadProgressMessageFont(
      {"Roboto"}, gfx::Font::NORMAL, kDownloadProgressMessageFontSize,
      gfx::Font::Weight::NORMAL);
  constexpr int kDownloadProgressMessageHeight = 24;
  constexpr int kProgressBarHeight = 5;
  constexpr int kProgressBarTopMargin = 32;

  SetCanMinimize(true);
  // Removed margins so dialog insets specify it instead.
  set_margins(gfx::Insets());

  views::BoxLayout* layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical, kDialogInsets));

  views::View* upper_container_view = new views::View();
  upper_container_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets()));
  AddChildView(upper_container_view);

  views::View* lower_container_view = new views::View();
  lower_container_layout_ =
      lower_container_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  AddChildView(lower_container_view);

  views::ImageView* logo_image = new views::ImageView();
  logo_image->SetImageSize(kLogoImageSize);
  logo_image->SetImage(
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_LOGO_PLUGIN_VM_DEFAULT_192));
  logo_image->SetHorizontalAlignment(views::ImageView::Alignment::kLeading);
  upper_container_view->AddChildView(logo_image);

  title_label_ = new views::Label(GetTitle(), {kTitleFont});
  title_label_->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(kTitleHeight - kTitleFontSize, 0, 0, 0));
  title_label_->SetMultiLine(false);
  title_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  upper_container_view->AddChildView(title_label_.get());

  views::View* message_container_view = new views::View();
  message_container_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::TLBR(kMessageHeight - kMessageFontSize, 0, 0, 0)));
  upper_container_view->AddChildView(message_container_view);

  message_label_ = new views::Label(GetMessage(), {kMessageFont});
  message_label_->SetMultiLine(true);
  message_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  message_container_view->AddChildView(message_label_.get());

  learn_more_link_ = new views::Link(l10n_util::GetStringUTF16(IDS_LEARN_MORE));
  learn_more_link_->SetCallback(base::BindRepeating(
      &PluginVmInstallerView::OnLinkClicked, base::Unretained(this)));
  learn_more_link_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  message_container_view->AddChildView(learn_more_link_.get());

  progress_bar_ = new views::ProgressBar();
  progress_bar_->SetPreferredHeight(kProgressBarHeight);
  progress_bar_->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(kProgressBarTopMargin - kProgressBarHeight, 0, 0, 0));
  upper_container_view->AddChildView(progress_bar_.get());

  download_progress_message_label_ =
      new views::Label(std::u16string(), {kDownloadProgressMessageFont});
  download_progress_message_label_->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(
          kDownloadProgressMessageHeight - kDownloadProgressMessageFontSize, 0,
          0, 0));
  download_progress_message_label_->SetMultiLine(false);
  download_progress_message_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  upper_container_view->AddChildView(download_progress_message_label_.get());

  big_image_ = new views::ImageView();
  lower_container_view->AddChildView(big_image_.get());

  // Make sure the lower_container_view is pinned to the bottom of the dialog.
  lower_container_layout_->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kEnd);
  layout->SetFlexForView(lower_container_view, 1, true);
}

// static
PluginVmInstallerView* PluginVmInstallerView::GetActiveViewForTesting() {
  return g_plugin_vm_installer_view;
}

bool PluginVmInstallerView::ShouldShowCloseButton() const {
  return true;
}

bool PluginVmInstallerView::ShouldShowWindowTitle() const {
  return false;
}

bool PluginVmInstallerView::Accept() {
  LOG_FUNCTION_CALL();
  if (state_ == State::kConfirmInstall) {
    delete learn_more_link_;
    learn_more_link_ = nullptr;
    StartInstallation();
    return false;
  }

  if (state_ == State::kCreated || state_ == State::kImported) {
    // Launch button has been clicked.
    plugin_vm::PluginVmManagerFactory::GetForProfile(profile_)->LaunchPluginVm(
        base::DoNothing());
    return true;
  }
  DCHECK_EQ(state_, State::kError);
  // Retry button has been clicked to retry setting of PluginVm environment
  // after error occurred.
  StartInstallation();
  return false;
}

bool PluginVmInstallerView::Cancel() {
  LOG_FUNCTION_CALL();
  return true;
}

gfx::Size PluginVmInstallerView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return gfx::Size(kWindowWidth, kWindowHeight);
}

void PluginVmInstallerView::OnStateUpdated(InstallingState new_state) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(state_, State::kInstalling);
  DCHECK_NE(new_state, InstallingState::kInactive);
  installing_state_ = new_state;
  OnStateUpdated();
}

void PluginVmInstallerView::OnLinkClicked() {
  NavigateParams params(profile_,
                        GURL("https://support.google.com/chrome/a/?p=pluginvm"),
                        ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);
}

void PluginVmInstallerView::OnProgressUpdated(double fraction_complete) {
  progress_bar_->SetValue(fraction_complete);
}

void PluginVmInstallerView::OnDownloadProgressUpdated(uint64_t bytes_downloaded,
                                                      int64_t content_length) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(installing_state_, InstallingState::kDownloadingImage);

  download_progress_message_label_->SetText(
      GetDownloadProgressMessage(bytes_downloaded, content_length));
}

void PluginVmInstallerView::OnVmExists() {
  DCHECK_EQ(installing_state_, InstallingState::kCheckingForExistingVm);

  state_ = State::kImported;
  installing_state_ = InstallingState::kInactive;
  OnStateUpdated();
  // Launch app now if the VM has previously been imported via
  // 'vmc import -p PvmDefault image.zip'.
  AcceptDialog();
}

void PluginVmInstallerView::OnCreated() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(installing_state_, InstallingState::kImporting);

  state_ = State::kCreated;
  installing_state_ = InstallingState::kInactive;
  OnStateUpdated();
}

void PluginVmInstallerView::OnImported() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(installing_state_, InstallingState::kImporting);

  state_ = State::kImported;
  installing_state_ = InstallingState::kInactive;
  OnStateUpdated();
}

void PluginVmInstallerView::OnError(
    plugin_vm::PluginVmInstaller::FailureReason reason) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  state_ = State::kError;
  installing_state_ = InstallingState::kInactive;
  reason_ = reason;
  OnStateUpdated();
}

// TODO(timloh): Cancelling the installation immediately closes the dialog, but
// getting back to a clean state could take several seconds. If a user then
// re-opens the dialog, it could cause it to fail unexpectedly. We should make
// use of these callback to avoid this.
void PluginVmInstallerView::OnCancelFinished() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

std::u16string PluginVmInstallerView::GetTitle() const {
  switch (state_) {
    case State::kConfirmInstall:
      return l10n_util::GetStringFUTF16(
          IDS_PLUGIN_VM_INSTALLER_CONFIRMATION_TITLE, app_name_);
    case State::kInstalling:
      return l10n_util::GetStringFUTF16(
          IDS_PLUGIN_VM_INSTALLER_ENVIRONMENT_SETTING_TITLE, app_name_);
    case State::kCreated:
    case State::kImported:
      return l10n_util::GetStringUTF16(IDS_PLUGIN_VM_INSTALLER_FINISHED_TITLE);
    case State::kError:
      DCHECK(reason_);
      switch (*reason_) {
        case plugin_vm::PluginVmInstaller::FailureReason::OFFLINE:
          return l10n_util::GetStringFUTF16(
              IDS_PLUGIN_VM_INSTALLER_ERROR_OFFLINE_TITLE,
              ui::GetChromeOSDeviceName());
        case plugin_vm::PluginVmInstaller::FailureReason::NOT_ALLOWED:
          return l10n_util::GetStringFUTF16(
              IDS_PLUGIN_VM_INSTALLER_NOT_ALLOWED_TITLE, app_name_);
        default:
          return l10n_util::GetStringUTF16(IDS_PLUGIN_VM_INSTALLER_ERROR_TITLE);
      }
  }
}

std::u16string PluginVmInstallerView::GetMessage() const {
  switch (state_) {
    case State::kConfirmInstall:
      return l10n_util::GetStringFUTF16(
          IDS_PLUGIN_VM_INSTALLER_CONFIRMATION_MESSAGE,
          ui::FormatBytesWithUnits(
              plugin_vm_installer_->RequiredFreeDiskSpace(),
              ui::DATA_UNITS_GIBIBYTE,
              /*show_units=*/true));
    case State::kInstalling:
      switch (installing_state_) {
        case InstallingState::kInactive:
          NOTREACHED();
        case InstallingState::kCheckingLicense:
        case InstallingState::kCheckingForExistingVm:
        case InstallingState::kCheckingDiskSpace:
        case InstallingState::kDownloadingDlc:
        case InstallingState::kStartingDispatcher:
          return l10n_util::GetStringUTF16(
              IDS_PLUGIN_VM_INSTALLER_START_DOWNLOADING_MESSAGE);
        case InstallingState::kDownloadingImage:
          return l10n_util::GetStringUTF16(
              IDS_PLUGIN_VM_INSTALLER_DOWNLOADING_MESSAGE);
        case InstallingState::kImporting:
          return l10n_util::GetStringUTF16(
              IDS_PLUGIN_VM_INSTALLER_IMPORTING_MESSAGE);
      }
    case State::kImported:
      return l10n_util::GetStringFUTF16(
          IDS_PLUGIN_VM_INSTALLER_IMPORTED_MESSAGE, app_name_);
    case State::kCreated:
      return l10n_util::GetStringUTF16(IDS_PLUGIN_VM_INSTALLER_CREATED_MESSAGE);
    case State::kError:
      using Reason = plugin_vm::PluginVmInstaller::FailureReason;
      DCHECK(reason_);
      switch (*reason_) {
        default:
        case Reason::SIGNAL_NOT_CONNECTED:
        case Reason::OPERATION_IN_PROGRESS:
        case Reason::UNEXPECTED_DISK_IMAGE_STATUS:
        case Reason::INVALID_DISK_IMAGE_STATUS_RESPONSE:
        case Reason::DISPATCHER_NOT_AVAILABLE:
        case Reason::CONCIERGE_NOT_AVAILABLE:
          return l10n_util::GetStringFUTF16(
              IDS_PLUGIN_VM_INSTALLER_ERROR_MESSAGE_LOGIC_ERROR, app_name_,
              base::NumberToString16(
                  static_cast<std::underlying_type_t<Reason>>(*reason_)));
        case Reason::EXISTING_IMAGE_INVALID:
          return l10n_util::GetStringFUTF16(
              IDS_PLUGIN_VM_INSTALLER_ERROR_INVALID_IMAGE_MESSAGE,
              base::UTF8ToUTF16(plugin_vm::kPluginVmName), app_name_);
        case Reason::OFFLINE:
          return l10n_util::GetStringUTF16(
              IDS_PLUGIN_VM_INSTALLER_ERROR_OFFLINE_MESSAGE);
        case Reason::NOT_ALLOWED:
        case Reason::DLC_UNSUPPORTED:
          return l10n_util::GetStringFUTF16(
              IDS_PLUGIN_VM_INSTALLER_NOT_ALLOWED_MESSAGE, app_name_,
              base::NumberToString16(
                  static_cast<std::underlying_type_t<Reason>>(*reason_)));
        case Reason::INVALID_LICENSE:
        case Reason::INVALID_IMAGE_URL:
        case Reason::HASH_MISMATCH:
          return l10n_util::GetStringFUTF16(
              IDS_PLUGIN_VM_INSTALLER_ERROR_MESSAGE_CONFIG_ERROR, app_name_,
              base::NumberToString16(
                  static_cast<std::underlying_type_t<Reason>>(*reason_)));
        case Reason::DOWNLOAD_FAILED_401:
        case Reason::DOWNLOAD_FAILED_403:
        case Reason::DOWNLOAD_FAILED_404:
          return l10n_util::GetStringFUTF16(
              IDS_PLUGIN_VM_INSTALLER_ERROR_MESSAGE_DOWNLOAD_HTTP_ERROR,
              app_name_,
              base::NumberToString16(HttpErrorFailureReasonToInt(*reason_)));
        case Reason::DOWNLOAD_FAILED_UNKNOWN:
        case Reason::DOWNLOAD_FAILED_NETWORK:
        case Reason::DOWNLOAD_FAILED_ABORTED:
        case Reason::DOWNLOAD_SIZE_MISMATCH:
          return l10n_util::GetStringFUTF16(
              IDS_PLUGIN_VM_INSTALLER_ERROR_MESSAGE_DOWNLOAD_FAILED,
              base::NumberToString16(
                  static_cast<std::underlying_type_t<Reason>>(*reason_)));
        case Reason::COULD_NOT_OPEN_IMAGE:
        case Reason::INVALID_IMPORT_RESPONSE:
        case Reason::IMAGE_IMPORT_FAILED:
          return l10n_util::GetStringFUTF16(
              IDS_PLUGIN_VM_INSTALLER_ERROR_MESSAGE_INSTALLING_FAILED,
              base::NumberToString16(
                  static_cast<std::underlying_type_t<Reason>>(*reason_)));
        // DLC Failure Reasons.
        case Reason::DLC_INTERNAL:
          return l10n_util::GetStringFUTF16(
              IDS_PLUGIN_VM_DLC_INTERNAL_FAILED_MESSAGE, app_name_);
        case Reason::DLC_BUSY:
          return l10n_util::GetStringFUTF16(
              IDS_PLUGIN_VM_DLC_BUSY_FAILED_MESSAGE, app_name_);
        case Reason::DLC_NEED_REBOOT:
          return l10n_util::GetStringFUTF16(
              IDS_PLUGIN_VM_DLC_NEED_REBOOT_FAILED_MESSAGE, app_name_);
        case Reason::INSUFFICIENT_DISK_SPACE:
        case Reason::DLC_NEED_SPACE:
        case Reason::OUT_OF_DISK_SPACE:
          return l10n_util::GetStringFUTF16(
              IDS_PLUGIN_VM_INSUFFICIENT_DISK_SPACE_MESSAGE,
              ui::FormatBytesWithUnits(
                  plugin_vm_installer_->RequiredFreeDiskSpace(),
                  ui::DATA_UNITS_GIBIBYTE,
                  /*show_units=*/true),
              app_name_);
      }
  }
}

void PluginVmInstallerView::SetFinishedCallbackForTesting(
    base::OnceCallback<void(bool success)> callback) {
  finished_callback_for_testing_ = std::move(callback);
}

PluginVmInstallerView::~PluginVmInstallerView() {
  VLOG(2) << "PluginVmInstallerView destroyed";
  plugin_vm_installer_->RemoveObserver();
  // We call |Cancel()| if the user hasn't started installation to log to UMA.
  if (state_ == State::kConfirmInstall || state_ == State::kInstalling)
    plugin_vm_installer_->Cancel();
  g_plugin_vm_installer_view = nullptr;
}

int PluginVmInstallerView::GetCurrentDialogButtons() const {
  switch (state_) {
    case State::kInstalling:
      return static_cast<int>(ui::mojom::DialogButton::kCancel);
    case State::kConfirmInstall:
    case State::kImported:
    case State::kCreated:
      return static_cast<int>(ui::mojom::DialogButton::kCancel) |
             static_cast<int>(ui::mojom::DialogButton::kOk);
    case State::kError:
      DCHECK(reason_);
      if (ShowRetryButton(*reason_))
        return static_cast<int>(ui::mojom::DialogButton::kCancel) |
               static_cast<int>(ui::mojom::DialogButton::kOk);
      return static_cast<int>(ui::mojom::DialogButton::kCancel);
  }
}

std::u16string PluginVmInstallerView::GetCurrentDialogButtonLabel(
    ui::mojom::DialogButton button) const {
  switch (state_) {
    case State::kConfirmInstall:
      return l10n_util::GetStringUTF16(
          button == ui::mojom::DialogButton::kOk
              ? IDS_PLUGIN_VM_INSTALLER_INSTALL_BUTTON
              : IDS_APP_CANCEL);
    case State::kInstalling:
      DCHECK_EQ(button, ui::mojom::DialogButton::kCancel);
      return l10n_util::GetStringUTF16(IDS_APP_CANCEL);
    case State::kCreated:
    case State::kImported: {
      return l10n_util::GetStringUTF16(
          button == ui::mojom::DialogButton::kOk
              ? IDS_PLUGIN_VM_INSTALLER_LAUNCH_BUTTON
              : IDS_APP_CLOSE);
    }
    case State::kError: {
      DCHECK(reason_);
      DCHECK(ShowRetryButton(*reason_) ||
             button == ui::mojom::DialogButton::kCancel);
      return l10n_util::GetStringUTF16(
          button == ui::mojom::DialogButton::kOk
              ? IDS_PLUGIN_VM_INSTALLER_RETRY_BUTTON
              : IDS_APP_CANCEL);
    }
  }
}

void PluginVmInstallerView::AddedToWidget() {
  // At this point GetWidget() is guaranteed to return non-null.
  OnStateUpdated();
}

void PluginVmInstallerView::OnThemeChanged() {
  views::BubbleDialogDelegateView::OnThemeChanged();
  download_progress_message_label_->SetEnabledColor(
      GetColorProvider()->GetColor(ui::kColorSecondaryForeground));
}

void PluginVmInstallerView::OnStateUpdated() {
  LOG_FUNCTION_CALL() << " with state_ = " << static_cast<int>(state_)
                      << ", installing_state_ = "
                      << static_cast<int>(installing_state_);
  SetTitleLabel();
  SetMessageLabel();
  SetBigImage();

  int buttons = GetCurrentDialogButtons();
  SetButtons(buttons);
  if (buttons & static_cast<int>(ui::mojom::DialogButton::kOk)) {
    SetButtonLabel(ui::mojom::DialogButton::kOk,
                   GetCurrentDialogButtonLabel(ui::mojom::DialogButton::kOk));
  }
  if (buttons & static_cast<int>(ui::mojom::DialogButton::kCancel)) {
    SetButtonLabel(
        ui::mojom::DialogButton::kCancel,
        GetCurrentDialogButtonLabel(ui::mojom::DialogButton::kCancel));
  }

  const bool progress_bar_visible = state_ == State::kInstalling;
  progress_bar_->SetVisible(progress_bar_visible);

  const bool download_progress_message_label_visible =
      installing_state_ == InstallingState::kDownloadingImage;
  download_progress_message_label_->SetVisible(
      download_progress_message_label_visible);

  DialogModelChanged();
  GetWidget()->GetRootView()->DeprecatedLayoutImmediately();

  if (state_ == State::kCreated || state_ == State::kImported ||
      state_ == State::kError) {
    if (finished_callback_for_testing_)
      std::move(finished_callback_for_testing_).Run(state_ != State::kError);
  }
}

std::u16string PluginVmInstallerView::GetDownloadProgressMessage(
    uint64_t bytes_downloaded,
    int64_t content_length) const {
  DCHECK_EQ(installing_state_, InstallingState::kDownloadingImage);

  // If download size isn't known |fraction_complete| should be empty.
  if (content_length > 0) {
    return l10n_util::GetStringFUTF16(
        IDS_PLUGIN_VM_INSTALLER_DOWNLOAD_PROGRESS_MESSAGE,
        ui::FormatBytesWithUnits(bytes_downloaded, ui::DATA_UNITS_GIBIBYTE,
                                 /*show_units=*/false),
        ui::FormatBytesWithUnits(content_length, ui::DATA_UNITS_GIBIBYTE,
                                 /*show_units=*/true));
  } else {
    return ui::FormatBytesWithUnits(bytes_downloaded, ui::DATA_UNITS_GIBIBYTE,
                                    /*show_units=*/true);
  }
}

void PluginVmInstallerView::SetTitleLabel() {
  title_label_->SetText(GetTitle());
  title_label_->SetVisible(true);
}

void PluginVmInstallerView::SetMessageLabel() {
  message_label_->SetText(GetMessage());
  message_label_->SetVisible(true);
}

void PluginVmInstallerView::SetBigImage() {
  constexpr gfx::Size kRegularImageSize(314, 191);
  constexpr gfx::Size kErrorImageSize(264, 264);
  constexpr int kRegularImageBottomInset = 52 + 57;
  constexpr int kErrorImageBottomInset = 52;

  auto setImage = [this](int image_id, gfx::Size size, int bottom_inset) {
    big_image_->SetImageSize(size);
    lower_container_layout_->set_inside_border_insets(
        gfx::Insets::TLBR(0, 0, bottom_inset, 0));
    big_image_->SetImage(
        ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(image_id));
  };

  if (state_ == State::kError) {
    setImage(IDR_PLUGIN_VM_INSTALLER_ERROR, kErrorImageSize,
             kErrorImageBottomInset);
    return;
  }
  setImage(IDR_PLUGIN_VM_INSTALLER, kRegularImageSize,
           kRegularImageBottomInset);
}

void PluginVmInstallerView::StartInstallation() {
  LOG_FUNCTION_CALL();
  state_ = State::kInstalling;
  installing_state_ = InstallingState::kCheckingLicense;
  progress_bar_->SetValue(0);
  download_progress_message_label_->SetText(std::u16string());
  OnStateUpdated();

  plugin_vm_installer_->SetObserver(this);
  std::optional<plugin_vm::PluginVmInstaller::FailureReason> failure_reason =
      plugin_vm_installer_->Start();
  if (failure_reason)
    OnError(failure_reason.value());
}

BEGIN_METADATA(PluginVmInstallerView)
ADD_READONLY_PROPERTY_METADATA(std::u16string, Title)
ADD_READONLY_PROPERTY_METADATA(std::u16string, Message)
END_METADATA

#undef LOG_FUNCTION_CALL
