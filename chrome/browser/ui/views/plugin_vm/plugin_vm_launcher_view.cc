// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/plugin_vm/plugin_vm_launcher_view.h"

#include <memory>

#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_image_manager.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_image_manager_factory.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_manager.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_metrics_util.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/window/dialog_client_view.h"

namespace {

PluginVmLauncherView* g_plugin_vm_launcher_view = nullptr;

constexpr gfx::Insets kButtonRowInsets(0, 64, 32, 64);
constexpr int kWindowWidth = 768;
constexpr int kWindowHeight = 636;

base::Optional<double> GetFractionComplete(double units_processed,
                                           double total_units) {
  if (total_units <= 0)
    return base::nullopt;
  double fraction_complete = units_processed / total_units;
  if (fraction_complete < 0.0 || fraction_complete > 1.0)
    return base::nullopt;
  return base::make_optional(fraction_complete);
}

}  // namespace

void plugin_vm::ShowPluginVmLauncherView(Profile* profile) {
  if (!g_plugin_vm_launcher_view) {
    g_plugin_vm_launcher_view = new PluginVmLauncherView(profile);
    views::DialogDelegate::CreateDialogWidget(g_plugin_vm_launcher_view,
                                              nullptr, nullptr);
  }
  g_plugin_vm_launcher_view->GetDialogClientView()->SetButtonRowInsets(
      kButtonRowInsets);
  g_plugin_vm_launcher_view->GetWidget()->Show();
}

PluginVmLauncherView::PluginVmLauncherView(Profile* profile)
    : profile_(profile),
      plugin_vm_image_manager_(
          plugin_vm::PluginVmImageManagerFactory::GetForProfile(profile)) {
  // Layout constants from the spec.
  gfx::Insets kDialogInsets(60, 64, 0, 64);
  constexpr gfx::Insets kLowerContainerInsets(12, 0, 52, 0);
  constexpr gfx::Size kLogoImageSize(32, 32);
  constexpr gfx::Size kBigImageSize(264, 264);
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
  views::BoxLayout* lower_container_layout =
      lower_container_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical, kLowerContainerInsets));
  AddChildView(lower_container_view);

  views::ImageView* logo_image = new views::ImageView();
  logo_image->SetImageSize(kLogoImageSize);
  logo_image->SetImage(
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_LOGO_PLUGIN_VM_DEFAULT_32));
  logo_image->SetHorizontalAlignment(views::ImageView::Alignment::kLeading);
  upper_container_view->AddChildView(logo_image);

  big_message_label_ = new views::Label(GetBigMessage(), {kTitleFont});
  big_message_label_->SetProperty(
      views::kMarginsKey, gfx::Insets(kTitleHeight - kTitleFontSize, 0, 0, 0));
  big_message_label_->SetMultiLine(false);
  big_message_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  upper_container_view->AddChildView(big_message_label_);

  views::View* message_container_view = new views::View();
  views::BoxLayout* message_container_layout =
      message_container_view->SetLayoutManager(
          std::make_unique<views::BoxLayout>(
              views::BoxLayout::Orientation::kHorizontal,
              gfx::Insets(kMessageHeight - kMessageFontSize, 0, 0, 0)));
  upper_container_view->AddChildView(message_container_view);

  message_label_ = new views::Label(GetMessage(), {kMessageFont});
  message_label_->SetMultiLine(false);
  message_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  message_container_view->AddChildView(message_label_);

  time_left_message_label_ = new views::Label(base::string16(), {kMessageFont});
  time_left_message_label_->SetEnabledColor(gfx::kGoogleGrey700);
  time_left_message_label_->SetMultiLine(false);
  time_left_message_label_->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
  message_container_view->AddChildView(time_left_message_label_);
  message_container_layout->SetFlexForView(time_left_message_label_, 1);

  progress_bar_ = new views::ProgressBar(kProgressBarHeight);
  progress_bar_->SetProperty(
      views::kMarginsKey,
      gfx::Insets(kProgressBarTopMargin - kProgressBarHeight, 0, 0, 0));
  upper_container_view->AddChildView(progress_bar_);

  download_progress_message_label_ =
      new views::Label(base::string16(), {kDownloadProgressMessageFont});
  download_progress_message_label_->SetEnabledColor(gfx::kGoogleGrey700);
  download_progress_message_label_->SetProperty(
      views::kMarginsKey, gfx::Insets(kDownloadProgressMessageHeight -
                                          kDownloadProgressMessageFontSize,
                                      0, 0, 0));
  download_progress_message_label_->SetMultiLine(false);
  download_progress_message_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  upper_container_view->AddChildView(download_progress_message_label_);

  big_image_ = new views::ImageView();
  big_image_->SetImageSize(kBigImageSize);
  big_image_->SetImage(
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_PLUGIN_VM_LAUNCHER));
  lower_container_view->AddChildView(big_image_);

  // Make sure the lower_container_view is pinned to the bottom of the dialog.
  lower_container_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kEnd);
  layout->SetFlexForView(lower_container_view, 1, true);
}

// static
PluginVmLauncherView* PluginVmLauncherView::GetActiveViewForTesting() {
  return g_plugin_vm_launcher_view;
}

int PluginVmLauncherView::GetDialogButtons() const {
  switch (state_) {
    case State::START_DOWNLOADING:
    case State::DOWNLOADING:
    case State::IMPORTING:
      return ui::DIALOG_BUTTON_CANCEL;
    case State::FINISHED:
      return ui::DIALOG_BUTTON_OK;
    case State::ERROR:
      DCHECK(reason_);
      switch (*reason_) {
        case plugin_vm::PluginVmImageManager::FailureReason::NOT_ALLOWED:
          return ui::DIALOG_BUTTON_CANCEL;
        default:
          return ui::DIALOG_BUTTON_CANCEL | ui::DIALOG_BUTTON_OK;
      }
  }
}

base::string16 PluginVmLauncherView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  switch (state_) {
    case State::START_DOWNLOADING:
    case State::DOWNLOADING:
    case State::IMPORTING: {
      DCHECK_EQ(button, ui::DIALOG_BUTTON_CANCEL);
      return l10n_util::GetStringUTF16(IDS_APP_CANCEL);
    }
    case State::FINISHED: {
      DCHECK_EQ(button, ui::DIALOG_BUTTON_OK);
      return l10n_util::GetStringUTF16(IDS_PLUGIN_VM_LAUNCHER_LAUNCH_BUTTON);
    }
    case State::ERROR: {
      DCHECK(reason_);
      switch (*reason_) {
        case plugin_vm::PluginVmImageManager::FailureReason::NOT_ALLOWED:
          DCHECK_EQ(button, ui::DIALOG_BUTTON_CANCEL);
          return l10n_util::GetStringUTF16(IDS_APP_CANCEL);
        default:
          return l10n_util::GetStringUTF16(
              button == ui::DIALOG_BUTTON_OK
                  ? IDS_PLUGIN_VM_LAUNCHER_RETRY_BUTTON
                  : IDS_APP_CANCEL);
      }
    }
  }
}

bool PluginVmLauncherView::ShouldShowWindowTitle() const {
  return false;
}

bool PluginVmLauncherView::Accept() {
  if (state_ == State::FINISHED) {
    // Launch button has been clicked.
    plugin_vm::PluginVmManager::GetForProfile(profile_)->LaunchPluginVm();
    return true;
  }
  DCHECK_EQ(state_, State::ERROR);
  // Retry button has been clicked to retry setting of PluginVm environment
  // after error occurred.
  StartPluginVmImageDownload();
  return false;
}

bool PluginVmLauncherView::Cancel() {
  if (state_ == State::DOWNLOADING || state_ == State::START_DOWNLOADING) {
    plugin_vm_image_manager_->CancelDownload();

    plugin_vm::RecordPluginVmSetupResultHistogram(
        plugin_vm::PluginVmSetupResult::kUserCancelledDownloadingPluginVmImage);
  }
  if (state_ == State::IMPORTING) {
    plugin_vm_image_manager_->CancelImport();

    plugin_vm::RecordPluginVmSetupResultHistogram(
        plugin_vm::PluginVmSetupResult::kUserCancelledImportingPluginVmImage);
  }

  return true;
}

gfx::Size PluginVmLauncherView::CalculatePreferredSize() const {
  return gfx::Size(kWindowWidth, kWindowHeight);
}

void PluginVmLauncherView::OnDownloadStarted() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  state_ = State::DOWNLOADING;
  OnStateUpdated();
}

void PluginVmLauncherView::OnDownloadProgressUpdated(
    uint64_t bytes_downloaded,
    int64_t content_length,
    base::TimeDelta elapsed_time) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(state_, State::DOWNLOADING);

  download_progress_message_label_->SetText(
      GetDownloadProgressMessage(bytes_downloaded, content_length));
  UpdateOperationProgress(bytes_downloaded, content_length, elapsed_time);
}

void PluginVmLauncherView::OnDownloadCompleted() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(state_, State::DOWNLOADING);

  plugin_vm_image_manager_->StartImport();
  state_ = State::IMPORTING;
  OnStateUpdated();
}

void PluginVmLauncherView::OnDownloadCancelled() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void PluginVmLauncherView::OnDownloadFailed(
    plugin_vm::PluginVmImageManager::FailureReason reason) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  state_ = State::ERROR;
  reason_ = reason;
  OnStateUpdated();

  plugin_vm::RecordPluginVmSetupResultHistogram(
      plugin_vm::PluginVmSetupResult::kErrorDownloadingPluginVmImage);
}

void PluginVmLauncherView::OnImportProgressUpdated(
    int percent_completed,
    base::TimeDelta elapsed_time) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(state_, State::IMPORTING);

  UpdateOperationProgress(percent_completed, 100.0, elapsed_time);
}

void PluginVmLauncherView::OnImportCancelled() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void PluginVmLauncherView::OnImportFailed(
    plugin_vm::PluginVmImageManager::FailureReason reason) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  state_ = State::ERROR;
  reason_ = reason;
  OnStateUpdated();

  plugin_vm::RecordPluginVmSetupResultHistogram(
      plugin_vm::PluginVmSetupResult::kErrorImportingPluginVmImage);
}

void PluginVmLauncherView::OnImported() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(state_, State::IMPORTING);

  state_ = State::FINISHED;
  OnStateUpdated();

  plugin_vm::RecordPluginVmSetupResultHistogram(
      plugin_vm::PluginVmSetupResult::kSuccess);
  plugin_vm::RecordPluginVmSetupTimeHistogram(base::TimeTicks::Now() -
                                              setup_start_tick_);
}

base::string16 PluginVmLauncherView::GetBigMessage() const {
  switch (state_) {
    case State::START_DOWNLOADING:
    case State::DOWNLOADING:
    case State::IMPORTING:
      return l10n_util::GetStringUTF16(
          IDS_PLUGIN_VM_LAUNCHER_ENVIRONMENT_SETTING_TITLE);
    case State::FINISHED:
      return l10n_util::GetStringUTF16(IDS_PLUGIN_VM_LAUNCHER_FINISHED_TITLE);
    case State::ERROR:
      DCHECK(reason_);
      switch (*reason_) {
        case plugin_vm::PluginVmImageManager::FailureReason::NOT_ALLOWED:
          return l10n_util::GetStringUTF16(
              IDS_PLUGIN_VM_LAUNCHER_NOT_ALLOWED_TITLE);
        default:
          return l10n_util::GetStringUTF16(IDS_PLUGIN_VM_LAUNCHER_ERROR_TITLE);
      }
  }
}

base::string16 PluginVmLauncherView::GetMessage() const {
  switch (state_) {
    case State::START_DOWNLOADING:
      return l10n_util::GetStringUTF16(
          IDS_PLUGIN_VM_LAUNCHER_START_DOWNLOADING_MESSAGE);
    case State::DOWNLOADING:
      return l10n_util::GetStringUTF16(
          IDS_PLUGIN_VM_LAUNCHER_DOWNLOADING_MESSAGE);
    case State::IMPORTING:
      return l10n_util::GetStringUTF16(
          IDS_PLUGIN_VM_LAUNCHER_IMPORTING_MESSAGE);
    case State::FINISHED:
      return l10n_util::GetStringUTF16(IDS_PLUGIN_VM_LAUNCHER_FINISHED_MESSAGE);
    case State::ERROR:
      DCHECK(reason_);
      switch (*reason_) {
        case plugin_vm::PluginVmImageManager::FailureReason::NOT_ALLOWED:
          return l10n_util::GetStringUTF16(
              IDS_PLUGIN_VM_LAUNCHER_NOT_ALLOWED_MESSAGE);
        default:
          return l10n_util::GetStringUTF16(
              IDS_PLUGIN_VM_LAUNCHER_ERROR_MESSAGE);
      }
  }
}

void PluginVmLauncherView::SetFinishedCallbackForTesting(
    base::OnceCallback<void(bool success)> callback) {
  finished_callback_for_testing_ = std::move(callback);
}

PluginVmLauncherView::~PluginVmLauncherView() {
  plugin_vm_image_manager_->RemoveObserver();
  g_plugin_vm_launcher_view = nullptr;
}

void PluginVmLauncherView::AddedToWidget() {
  // Defensive check that ensures an error message is shown if this
  // dialogue is reached somehow although PluginVm has been disabled.
  if (!plugin_vm::IsPluginVmAllowedForProfile(profile_)) {
    LOG(ERROR) << "PluginVm is disallowed by policy. Showing error screen.";
    state_ = State::ERROR;
    reason_ = plugin_vm::PluginVmImageManager::FailureReason::NOT_ALLOWED;
    plugin_vm::RecordPluginVmSetupResultHistogram(
        plugin_vm::PluginVmSetupResult::kPluginVmIsNotAllowed);
  }

  if (state_ == State::START_DOWNLOADING)
    StartPluginVmImageDownload();
  else
    OnStateUpdated();
}

void PluginVmLauncherView::OnStateUpdated() {
  // TODO(https://crbug.com/1017511): display failure reasons.
  SetBigMessageLabel();
  SetMessageLabel();
  SetBigImage();

  const bool progress_bar_visible = state_ == State::START_DOWNLOADING ||
                                    state_ == State::DOWNLOADING ||
                                    state_ == State::IMPORTING;
  progress_bar_->SetVisible(progress_bar_visible);
  // Values outside the range [0,1] display an infinite loading animation.
  progress_bar_->SetValue(-1);

  // This will be shown once we receive download/import progress messages.
  time_left_message_label_->SetVisible(false);

  const bool download_progress_message_label_visible =
      state_ == State::DOWNLOADING;
  download_progress_message_label_->SetVisible(
      download_progress_message_label_visible);

  DialogModelChanged();
  GetWidget()->GetRootView()->Layout();

  if (state_ == State::FINISHED || state_ == State::ERROR) {
    if (finished_callback_for_testing_)
      std::move(finished_callback_for_testing_).Run(state_ == State::FINISHED);
  }
}

base::string16 PluginVmLauncherView::GetDownloadProgressMessage(
    uint64_t bytes_downloaded,
    int64_t content_length) const {
  DCHECK_EQ(state_, State::DOWNLOADING);

  base::Optional<double> fraction_complete =
      GetFractionComplete(bytes_downloaded, content_length);

  // If download size isn't known |fraction_complete| should be empty.
  if (fraction_complete.has_value()) {
    return l10n_util::GetStringFUTF16(
        IDS_PLUGIN_VM_LAUNCHER_DOWNLOAD_PROGRESS_MESSAGE,
        ui::FormatBytesWithUnits(bytes_downloaded, ui::DATA_UNITS_GIBIBYTE,
                                 /*show_units=*/false),
        ui::FormatBytesWithUnits(content_length, ui::DATA_UNITS_GIBIBYTE,
                                 /*show_units=*/true));
  } else {
    return l10n_util::GetStringFUTF16(
        IDS_PLUGIN_VM_LAUNCHER_DOWNLOAD_PROGRESS_WITHOUT_DOWNLOAD_SIZE_MESSAGE,
        ui::FormatBytesWithUnits(bytes_downloaded, ui::DATA_UNITS_GIBIBYTE,
                                 /*show_units=*/true));
  }
}

void PluginVmLauncherView::UpdateOperationProgress(
    double units_processed,
    double total_units,
    base::TimeDelta elapsed_time) const {
  DCHECK(state_ == State::DOWNLOADING || state_ == State::IMPORTING);

  base::Optional<double> maybe_fraction_complete =
      GetFractionComplete(units_processed, total_units);

  if (!maybe_fraction_complete || units_processed == 0 ||
      elapsed_time.is_zero()) {
    progress_bar_->SetValue(-1);
    time_left_message_label_->SetVisible(false);
    return;
  }

  const double fraction_complete = *maybe_fraction_complete;
  const double fraction_remaining = 1 - fraction_complete;

  progress_bar_->SetValue(fraction_complete);
  time_left_message_label_->SetVisible(true);
  base::TimeDelta remaining =
      fraction_remaining / fraction_complete * elapsed_time;
  time_left_message_label_->SetText(
      ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_REMAINING,
                             ui::TimeFormat::LENGTH_SHORT, remaining));
}

void PluginVmLauncherView::SetBigMessageLabel() {
  big_message_label_->SetText(GetBigMessage());
  big_message_label_->SetVisible(true);
}

void PluginVmLauncherView::SetMessageLabel() {
  message_label_->SetText(GetMessage());
  message_label_->SetVisible(true);
}

void PluginVmLauncherView::SetBigImage() {
  if (state_ == State::ERROR) {
    big_image_->SetImage(
        ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            IDR_PLUGIN_VM_LAUNCHER_ERROR));
    return;
  }
  big_image_->SetImage(
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_PLUGIN_VM_LAUNCHER));
}

void PluginVmLauncherView::StartPluginVmImageDownload() {
  // In each case setup starts from this function (when dialog is opened or
  // retry button is clicked).
  setup_start_tick_ = base::TimeTicks::Now();

  state_ = State::START_DOWNLOADING;
  OnStateUpdated();

  plugin_vm_image_manager_->SetObserver(this);
  plugin_vm_image_manager_->StartDownload();
}
