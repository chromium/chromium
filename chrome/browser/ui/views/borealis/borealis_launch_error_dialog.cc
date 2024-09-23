// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/borealis/borealis_launch_error_dialog.h"

#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "chrome/browser/ash/borealis/borealis_app_launcher.h"
#include "chrome/browser/ash/borealis/borealis_features.h"
#include "chrome/browser/ash/borealis/borealis_metrics.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/borealis/borealis_service_factory.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chrome/browser/feedback/show_feedback_page.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/views/borealis/borealis_disallowed_dialog.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace views::borealis {

using ::borealis::BorealisStartupResult;

namespace {

// Views uses tricks like this to ensure singleton-ness of dialogs.
static Widget* g_instance_ = nullptr;

class BorealisLaunchErrorDialog : public DialogDelegate {
 public:
  explicit BorealisLaunchErrorDialog(Profile* profile,
                                     BorealisStartupResult error) {
    DCHECK(!g_instance_);

    SetTitle(IDS_BOREALIS_APP_NAME);
    set_internal_name("BorealisLaunchErrorDialog");

    failure_ = IdentifyFailure(error);

    std::u16string ok_string;
    switch (failure_) {
      case FailureType::FAILURE_NEED_RESTART:
        ok_string =
            l10n_util::GetStringUTF16(IDS_BOREALIS_LAUNCH_ERROR_BUTTON_RESTART);
        break;
      case FailureType::FAILURE_NEED_SPACE:
        ok_string = l10n_util::GetStringUTF16(
            IDS_BOREALIS_INSTALLER_ERROR_BUTTON_STORAGE);
        break;
      case FailureType::FAILURE_RETRY:
        ok_string = l10n_util::GetStringUTF16(
            IDS_BOREALIS_INSTALLER_ERROR_BUTTON_RETRY);
    }

    SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk) |
               static_cast<int>(ui::mojom::DialogButton::kCancel));
    SetButtonLabel(ui::mojom::DialogButton::kOk, ok_string);
    SetButtonLabel(ui::mojom::DialogButton::kCancel,
                   l10n_util::GetStringUTF16(IDS_APP_CANCEL));

    SetCancelCallback(base::BindOnce(&BorealisLaunchErrorDialog::OnCancelled,
                                     base::Unretained(this), profile));

    SetAcceptCallback(base::BindOnce(&BorealisLaunchErrorDialog::OnAccepted,
                                     base::Unretained(this), profile));

    InitializeView();
    SetModalType(ui::mojom::ModalType::kNone);
    SetOwnedByWidget(true);
    SetShowCloseButton(false);
    set_fixed_width(ChromeLayoutProvider::Get()->GetDistanceMetric(
        views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));
  }

  ~BorealisLaunchErrorDialog() override {
    DCHECK(g_instance_);
    g_instance_ = nullptr;
  }

  bool ShouldShowWindowTitle() const override { return false; }

 private:
  // Used to identify the failure type.
  enum FailureType {
    FAILURE_NEED_RESTART,
    FAILURE_NEED_SPACE,
    FAILURE_RETRY,
  };

  FailureType IdentifyFailure(BorealisStartupResult error) {
    switch (error) {
      case BorealisStartupResult::kDlcNeedRebootError:
      case BorealisStartupResult::kDlcNeedUpdateError:
        return FAILURE_NEED_RESTART;
      case BorealisStartupResult::kDlcNeedSpaceError:
        return FAILURE_NEED_SPACE;
      case BorealisStartupResult::kAwaitBorealisStartupFailed:
      case BorealisStartupResult::kDiskImageFailed:
      case BorealisStartupResult::kDlcCancelled:
      case BorealisStartupResult::kDlcOffline:
      case BorealisStartupResult::kDlcBusyError:
      case BorealisStartupResult::kDlcInternalError:
      case BorealisStartupResult::kStartVmEmptyResponse:
      case BorealisStartupResult::kStartVmFailed:
      case BorealisStartupResult::kEmptyDiskResponse:
      default:
        return FAILURE_RETRY;
    }
  }

  void InitializeView() {
    auto view = std::make_unique<views::View>();

    views::LayoutProvider* provider = views::LayoutProvider::Get();
    view->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical,
        provider->GetInsetsMetric(views::InsetsMetric::INSETS_DIALOG),
        provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));

    views::Label* title_label = new views::Label(
        l10n_util::GetStringUTF16(IDS_BOREALIS_LAUNCH_ERROR_TITLE),
        CONTEXT_IPH_BUBBLE_TITLE, views::style::STYLE_EMPHASIZED);
    title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    title_label->SetMultiLine(true);
    view->AddChildView(title_label);

    std::u16string body_string;
    switch (failure_) {
      case FailureType::FAILURE_NEED_RESTART:
        body_string = l10n_util::GetStringUTF16(
            IDS_BOREALIS_LAUNCH_ERROR_NEED_RESTART_BODY);
        break;
      case FailureType::FAILURE_NEED_SPACE:
        body_string = l10n_util::GetStringUTF16(
            IDS_BOREALIS_LAUNCH_ERROR_NEED_SPACE_BODY);
        break;
      case FailureType::FAILURE_RETRY:
        body_string = l10n_util::GetStringUTF16(IDS_BOREALIS_LAUNCH_ERROR_BODY);
    }
    views::Label* message_label = new views::Label(body_string);
    message_label->SetMultiLine(true);
    message_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    view->AddChildView(message_label);

    if (failure_ == FailureType::FAILURE_RETRY) {
      auto checkbox = std::make_unique<views::Checkbox>(
          l10n_util::GetStringUTF16(IDS_BOREALIS_LAUNCH_ERROR_SEND_FEEDBACK));
      feedback_checkbox_ = view->AddChildView(std::move(checkbox));
    }

    SetContentsView(std::move(view));
  }

  void OnCancelled(Profile* profile) {
    if (feedback_checkbox_ && feedback_checkbox_->GetChecked()) {
      ShowFeedbackPage(profile);
    }
  }

  void OnAccepted(Profile* profile) {
    if (feedback_checkbox_ && feedback_checkbox_->GetChecked()) {
      ShowFeedbackPage(profile);
    }

    if (failure_ == FailureType::FAILURE_NEED_RESTART) {
      chromeos::PowerManagerClient::Get()->RequestRestart(
          power_manager::REQUEST_RESTART_FOR_USER,
          "borealis failure need restart");
    } else if (failure_ == FailureType::FAILURE_NEED_SPACE) {
      chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
          profile, chromeos::settings::mojom::kStorageSubpagePath);
    } else if (failure_ == FailureType::FAILURE_RETRY) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(
                         [](Profile* profile) {
                           // Technically "retry" should re-do whatever the user
                           // originally tried. For simplicity we just retry the
                           // client app.
                           ::borealis::BorealisServiceFactory::GetForProfile(
                               profile)
                               ->AppLauncher()
                               .Launch(::borealis::kClientAppId,
                                       ::borealis::BorealisLaunchSource::
                                           kErrorDialogRetryButton,
                                       base::DoNothing());
                         },
                         profile));
    }
  }

  void ShowFeedbackPage(Profile* profile) {
    chrome::ShowFeedbackPage(
        /*page_url=*/GURL(), profile, feedback::kFeedbackSourceBorealis,
        /*description_template=*/std::string(),
        /*description_placeholder_text=*/
        l10n_util::GetStringUTF8(IDS_BOREALIS_FEEDBACK_PLACEHOLDER),
        /*category_tag=*/"borealis",
        /*extra_diagnostics=*/std::string());
  }

  FailureType failure_;
  raw_ptr<views::Checkbox> feedback_checkbox_ = nullptr;
};
}  // namespace

void ShowBorealisLaunchErrorView(Profile* profile,
                                 BorealisStartupResult error) {
  if (error == BorealisStartupResult::kDisallowed) {
    ::borealis::BorealisServiceFactory::GetForProfile(profile)
        ->Features()
        .IsAllowed(base::BindOnce(&ShowLauncherDisallowedDialog));
    return;
  }

  // TODO(b/248938308): Closing and reopening the dialog this way is not
  // desirable. When we move to webui we should just re-show the current dialog.
  if (g_instance_) {
    g_instance_->CloseNow();
  }

  auto delegate = std::make_unique<BorealisLaunchErrorDialog>(profile, error);
  g_instance_ = views::DialogDelegate::CreateDialogWidget(std::move(delegate),
                                                          nullptr, nullptr);
  g_instance_->GetNativeWindow()->SetProperty(
      ash::kShelfIDKey, ash::ShelfID(::borealis::kClientAppId).Serialize());
  g_instance_->Show();
}

}  // namespace views::borealis
