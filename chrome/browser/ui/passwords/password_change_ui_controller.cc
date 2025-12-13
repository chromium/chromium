// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/password_change_ui_controller.h"

#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/password_manager/password_change_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/passwords/password_change/password_change_toast.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/strings/grit/components_strings.h"
#include "components/tabs/public/tab_interface.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/vector_icons.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace {

using ToastOptions = PasswordChangeToast::ToastOptions;

constexpr char kLeakDetectionDialogHistogram[] =
    "PasswordManager.PasswordChange.LeakDetectionDialog";

// Logs `action` taken for the dialog displayed for `state`.
void LogDialogAction(PasswordChangeDelegate::State state,
                     PasswordChangeDialogAction action) {
  switch (state) {
    case PasswordChangeDelegate::State::kOfferingPasswordChange:
    case PasswordChangeDelegate::State::kWaitingForAgreement: {
      std::string suffix =
          state == PasswordChangeDelegate::State::kOfferingPasswordChange
              ? ".WithoutPrivacyNotice"
              : ".WithPrivacyNotice";
      base::UmaHistogramEnumeration(kLeakDetectionDialogHistogram, action);
      base::UmaHistogramEnumeration(
          base::StrCat({kLeakDetectionDialogHistogram, suffix}), action);
      return;
    }
    case PasswordChangeDelegate::State::kChangePasswordFormNotFound:
      base::UmaHistogramEnumeration(
          "PasswordManager.PasswordChange.NoPasswordForm", action);
      return;
    case PasswordChangeDelegate::State::kPasswordChangeFailed:
      base::UmaHistogramEnumeration(
          "PasswordManager.PasswordChange.FailedInteraction", action);
      return;
    case PasswordChangeDelegate::State::kOtpDetected:
      base::UmaHistogramEnumeration(
          "PasswordManager.PasswordChange.OTPRequested", action);
      return;
    case PasswordChangeDelegate::State::kWaitingForChangePasswordForm:
    case PasswordChangeDelegate::State::kChangingPassword:
    case PasswordChangeDelegate::State::kPasswordSuccessfullyChanged:
    case PasswordChangeDelegate::State::kCanceled:
    case PasswordChangeDelegate::State::kNoState:
    case PasswordChangeDelegate::State::kLoginFormDetected:
      NOTREACHED();
  }
}

// Logs `event` that happened for the toast displayed for `state`.
void LogToastEvent(PasswordChangeDelegate::State state,
                   PasswordChangeToastEvent event) {
  switch (state) {
    case PasswordChangeDelegate::State::kWaitingForChangePasswordForm:
      base::UmaHistogramEnumeration(
          "PasswordManager.PasswordChange.CheckingSignInToast", event);
      return;
    case PasswordChangeDelegate::State::kChangingPassword:
      base::UmaHistogramEnumeration(
          "PasswordManager.PasswordChange.ChangingPasswordToast", event);
      return;
    case PasswordChangeDelegate::State::kLoginFormDetected:
      base::UmaHistogramEnumeration(
          "PasswordManager.PasswordChange.WaitingForUserSignInToast", event);
      return;
    case PasswordChangeDelegate::State::kCanceled:
      base::UmaHistogramEnumeration(
          "PasswordManager.PasswordChange.CanceledToast", event);
      return;
    case PasswordChangeDelegate::State::kPasswordSuccessfullyChanged:
      // Events for this toast are already implicitly logged.
      return;
    case PasswordChangeDelegate::State::kOfferingPasswordChange:
    case PasswordChangeDelegate::State::kWaitingForAgreement:
    case PasswordChangeDelegate::State::kChangePasswordFormNotFound:
    case PasswordChangeDelegate::State::kPasswordChangeFailed:
    case PasswordChangeDelegate::State::kOtpDetected:
    case PasswordChangeDelegate::State::kNoState:
      NOTREACHED();
  }
}

void LogLeakDialogTimeSpent(bool with_privacy_notice, base::Time display_time) {
  std::string suffix =
      with_privacy_notice ? ".WithPrivacyNotice" : ".WithoutPrivacyNotice";
  base::UmaHistogramMediumTimes(
      base::StrCat(
          {"PasswordManager.PasswordChange.LeakDetectionDialog.TimeSpent",
           suffix}),
      base::Time::Now() - display_time);
}

// Creates dialog offering password change to the user. `with_privacy_notice`
// specifies whether an additional privacy paragraph should be displayed.
std::unique_ptr<ui::DialogModel> CreateOfferChangePasswordDialog(
    base::OnceClosure accept_callback,
    base::OnceClosure cancel_callback,
    base::RepeatingClosure navigate_to_settings_callback,
    bool with_privacy_notice,
    std::u16string email) {
  ui::DialogModel::Builder dialog_builder;
  dialog_builder.SetBannerImage(
      ui::ResourceBundle::GetSharedInstance().GetThemedLottieImageNamed(
          IDR_PASSWORD_CHANGE_WARNING_LOTTIE));
  dialog_builder.SetIcon(
      ui::ImageModel::FromVectorIcon(GooglePasswordManagerVectorIcon()));
  dialog_builder.SetTitle(l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_LEAK_DIALOG_TITLE));

  auto time_callback = base::BindRepeating(
      &LogLeakDialogTimeSpent, with_privacy_notice, base::Time::Now());
  dialog_builder.AddCancelButton(std::move(cancel_callback).Then(time_callback),
                                 ui::DialogModel::Button::Params().SetLabel(
                                     l10n_util::GetStringUTF16(IDS_NO_THANKS)));
  dialog_builder.AddOkButton(
      std::move(accept_callback).Then(time_callback),
      ui::DialogModel::Button::Params().SetLabel(l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_CHANGE_PASSWORD)));

  ui::DialogModelLabel::TextReplacement email_label =
      ui::DialogModelLabel::CreatePlainText(std::move(email));
  ui::DialogModelLabel::TextReplacement link = ui::DialogModelLabel::CreateLink(
      with_privacy_notice
          ? IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_LEAK_DIALOG_LINK_WITH_PRIVACY_NOTICE
          : IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_LEAK_DIALOG_LINK_WITHOUT_PRIVACY_NOTICE,
      std::move(navigate_to_settings_callback));
  if (with_privacy_notice) {
    dialog_builder.AddParagraph(ui::DialogModelLabel::CreateWithReplacements(
        IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_LEAK_DIALOG_DETAILS_WITH_PRIVACY_NOTICE,
        {email_label}));
    dialog_builder.AddParagraph(ui::DialogModelLabel::CreateWithReplacements(
        IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_PRIVACY_NOTICE, {link}));
  } else {
    dialog_builder.AddParagraph(ui::DialogModelLabel::CreateWithReplacements(
        IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_LEAK_DIALOG_DETAILS_WITHOUT_PRIVACY_NOTICE,
        {email_label, link}));
  }
  return dialog_builder.Build();
}

// Creates dialog for failed states of password change flow.
std::unique_ptr<ui::DialogModel> CreatePasswordChangeFailedDialog(
    base::OnceClosure accept_callback,
    base::OnceClosure cancel_callback,
    bool use_error_image) {
  auto image_model =
      ui::ResourceBundle::GetSharedInstance().GetThemedLottieImageNamed(
          use_error_image ? IDR_PASSWORD_CHANGE_WARNING_LOTTIE
                          : IDR_PASSWORD_CHANGE_NEUTRAL_LOTTIE);
  return ui::DialogModel::Builder()
      .SetBannerImage(image_model)
      .SetTitle(l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_FAILED_TITLE))
      .AddParagraph(ui::DialogModelLabel(l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_FAILED_BODY)))
      .AddCancelButton(std::move(cancel_callback),
                       ui::DialogModel::Button::Params().SetLabel(
                           l10n_util::GetStringUTF16(IDS_CLOSE)))
      .AddOkButton(
          std::move(accept_callback),
          ui::DialogModel::Button::Params().SetLabel(l10n_util::GetStringUTF16(
              IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_FAILED_ACCEPT_BUTTON)))
      .Build();
}

// Creates a BubbleFrameView to be used as the non-client frame view for the
// toast widget. This frame view provides rounded corners and a custom
// background color.
std::unique_ptr<views::FrameView> CreateToastFrameView(views::Widget* widget) {
  auto frame_view = std::make_unique<views::BubbleFrameView>(
      /*title_margins=*/gfx::Insets(), /*content_margins=*/gfx::Insets());
  auto border = std::make_unique<views::BubbleBorder>(
      views::BubbleBorder::Arrow::NONE,
      views::BubbleBorder::Shadow::STANDARD_SHADOW);
  const int corner_radius = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_TOAST_BUBBLE_HEIGHT);
  border->set_rounded_corners(gfx::RoundedCornersF(corner_radius));
  border->set_draw_border_stroke(false);
  frame_view->SetBubbleBorder(std::move(border));
  frame_view->SetBackgroundColor(ui::kColorToastBackgroundProminent);
  return frame_view;
}

// Creates dialog for `PasswordChangeDelegate::State::kOtpDetected`.
std::unique_ptr<ui::DialogModel> CreateOtpDetectedDialog(
    base::OnceClosure accept_callback,
    base::OnceClosure cancel_callback) {
  return ui::DialogModel::Builder()
      .SetBannerImage(
          ui::ResourceBundle::GetSharedInstance().GetThemedLottieImageNamed(
              IDR_PASSWORD_CHANGE_NEUTRAL_LOTTIE))
      .SetTitle(
          l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_UI_OTP_DIALOG_TITLE))
      .AddParagraph(ui::DialogModelLabel(l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_UI_OTP_DIALOG_DETAILS)))
      .AddCancelButton(std::move(cancel_callback),
                       ui::DialogModel::Button::Params().SetLabel(
                           l10n_util::GetStringUTF16(IDS_CANCEL)))
      .AddOkButton(std::move(accept_callback),
                   ui::DialogModel::Button::Params().SetLabel(
                       l10n_util::GetStringUTF16(IDS_CONTINUE)))
      .Build();
}

}  // namespace

PasswordChangeUIController::PasswordChangeUIController(
    PasswordChangeDelegate* password_change_delegate,
    tabs::TabInterface* tab_interface)
    : password_change_delegate_(password_change_delegate),
      tab_interface_(tab_interface) {}

PasswordChangeUIController::~PasswordChangeUIController() {
  CloseDialogWidget(views::Widget::ClosedReason::kUnspecified);
  CloseToastWidget(views::Widget::ClosedReason::kUnspecified);
}

void PasswordChangeUIController::UpdateState(
    PasswordChangeDelegate::State state) {
  if (state_ == state) {
    return;
  }

  state_ = state;
  std::variant<ToastOptions, std::unique_ptr<ui::DialogModel>> configuration =
      GetDialogOrToastConfiguration(state);

  if (std::holds_alternative<ToastOptions>(configuration)) {
    // Close the existing dialog before showing toast. This is needed in
    // PasswordChangeToastBrowserTest.InvokeUi_Toast.
    CloseDialogWidget(views::Widget::ClosedReason::kUnspecified);
    if (toast_view_) {
      // If already showing a toast, update its layout.
      toast_view_->UpdateLayout(std::move(std::get<ToastOptions>(configuration)));
      // Manually trigger a bounds update since the widget is not auto-sized.
      tab_interface_->GetTabFeatures()
          ->tab_dialog_manager()
          ->UpdateModalDialogBounds();
    } else {
      // If not showing a toast, show a new one.
      ShowToast(std::move(std::get<ToastOptions>(configuration)));
    }
    LogToastEvent(state_, PasswordChangeToastEvent::kShown);
    return;
  }

  // Close the toast before attempting to open any dialog.
  CloseToastWidget(views::Widget::ClosedReason::kUnspecified);
  ShowDialog(
      std::move(std::get<std::unique_ptr<ui::DialogModel>>(configuration)));
}

std::variant<ToastOptions, std::unique_ptr<ui::DialogModel>>
PasswordChangeUIController::GetDialogOrToastConfiguration(
    PasswordChangeDelegate::State state) {
  auto open_password_change_tab_from_dialog_callback = base::BindOnce(
      &PasswordChangeUIController::OpenPasswordChangeTabFromDialog,
      weak_ptr_factory_.GetWeakPtr());
  auto open_password_change_tab_from_toast_callback = base::BindOnce(
      &PasswordChangeUIController::OpenPasswordChangeTabFromToast,
      weak_ptr_factory_.GetWeakPtr());
  auto cancel_dialog_callback =
      base::BindOnce(&PasswordChangeUIController::OnDialogCanceled,
                     weak_ptr_factory_.GetWeakPtr());
  auto cancel_toast_callback =
      base::BindOnce(&PasswordChangeUIController::OnToastCanceled,
                     weak_ptr_factory_.GetWeakPtr());
  auto navigate_to_settings_callback = base::BindRepeating(
      &PasswordChangeUIController::NavigateToPasswordChangeSettings,
      base::Unretained(this));
  Profile* profile = Profile::FromBrowserContext(
      tab_interface_->GetContents()->GetBrowserContext());
  std::u16string email = base::UTF8ToUTF16(GetDisplayableAccountName(
      SyncServiceFactory::GetForProfile(profile),
      IdentityManagerFactory::GetForProfile(profile)));

  switch (state) {
    /* Dialogs */
    case PasswordChangeDelegate::State::kWaitingForAgreement:
      return CreateOfferChangePasswordDialog(
          base::BindOnce(&PasswordChangeUIController::OnPrivacyNoticeAccepted,
                         weak_ptr_factory_.GetWeakPtr()),
          std::move(cancel_dialog_callback),
          std::move(navigate_to_settings_callback),
          /*with_privacy_notice=*/true, std::move(email));
    case PasswordChangeDelegate::State::kOfferingPasswordChange:
      return CreateOfferChangePasswordDialog(
          base::BindOnce(&PasswordChangeUIController::StartPasswordChangeFlow,
                         weak_ptr_factory_.GetWeakPtr()),
          std::move(cancel_dialog_callback),
          std::move(navigate_to_settings_callback),
          /*with_privacy_notice=*/false, std::move(email));
    case PasswordChangeDelegate::State::kChangePasswordFormNotFound:
      return CreatePasswordChangeFailedDialog(
          std::move(open_password_change_tab_from_dialog_callback),
          std::move(cancel_dialog_callback),
          /*use_error_image=*/false);
    case PasswordChangeDelegate::State::kPasswordChangeFailed:
      return CreatePasswordChangeFailedDialog(
          std::move(open_password_change_tab_from_dialog_callback),
          std::move(cancel_dialog_callback),
          /*use_error_image=*/true);
    case PasswordChangeDelegate::State::kOtpDetected:
      return CreateOtpDetectedDialog(
          std::move(open_password_change_tab_from_dialog_callback),
          std::move(cancel_dialog_callback));

    /* Toasts */
    case PasswordChangeDelegate::State::kWaitingForChangePasswordForm:
      return ToastOptions(
          l10n_util::GetStringUTF16(
              IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_OMNIBOX_SIGN_IN_CHECK),
          std::move(cancel_toast_callback));
    case PasswordChangeDelegate::State::kChangingPassword:
      return ToastOptions(
          l10n_util::GetStringUTF16(
              IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_OMNIBOX_CHANGING_PASSWORD),
          std::move(cancel_toast_callback));
    case PasswordChangeDelegate::State::kPasswordSuccessfullyChanged:
      return ToastOptions(
          l10n_util::GetStringUTF16(
              IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGED_TITLE),
          views::kMenuCheckIcon, std::move(cancel_toast_callback),
          l10n_util::GetStringUTF16(
              IDS_PASSWORD_MANAGER_UI_VIEW_DETAILS_BUTTON),
          base::BindOnce(&PasswordChangeUIController::ShowPasswordDetails,
                         weak_ptr_factory_.GetWeakPtr()));
    case PasswordChangeDelegate::State::kCanceled:
      return ToastOptions(
          l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_UI_PASSWORD_UNCHANGED),
          vector_icons::kPasswordManagerOffIcon,
          std::move(cancel_toast_callback),
          l10n_util::GetStringUTF16(
              IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_FAILED_ACCEPT_BUTTON),
          std::move(open_password_change_tab_from_toast_callback));
    case PasswordChangeDelegate::State::kLoginFormDetected:
      return ToastOptions(
          l10n_util::GetStringUTF16(
              IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_TOAST_SIGN_IN_TO_CONTINUE),
          views::kInfoChromeRefreshIcon, std::move(cancel_toast_callback),
          l10n_util::GetStringUTF16(
              IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_TOAST_RETRY_BUTTON),
          base::BindOnce(&PasswordChangeUIController::RetryLoginCheck,
                         weak_ptr_factory_.GetWeakPtr()));
    case PasswordChangeDelegate::State::kNoState:
      NOTREACHED();
  }
}

void PasswordChangeUIController::ShowToast(ToastOptions options) {
  CHECK(tab_interface_);

  std::u16string title = options.text;
  auto toast_view = std::make_unique<PasswordChangeToast>(std::move(options));
  toast_view_ = toast_view.get();

  auto toast_delegate = std::make_unique<views::WidgetDelegate>();
  toast_delegate->SetModalType(ui::mojom::ModalType::kChild);
  toast_delegate->SetContentsView(std::move(toast_view));
  toast_delegate->SetAccessibleWindowRole(ax::mojom::Role::kAlert);
  toast_delegate->SetAccessibleTitle(title);
  toast_delegate->SetShowCloseButton(false);
  toast_delegate->SetFrameViewFactory(
      base::BindRepeating(&CreateToastFrameView));
  toast_delegate_ = std::move(toast_delegate);

  auto* tab_dialog_manager =
      tab_interface_->GetTabFeatures()->tab_dialog_manager();
  auto widget = std::make_unique<views::Widget>();
  views::Widget::InitParams init_params(
      views::Widget::InitParams::Ownership::CLIENT_OWNS_WIDGET);
  init_params.delegate = toast_delegate_.get();
  // Use translucency to enable rounded corners.
  init_params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  init_params.parent = tab_dialog_manager->GetHostWidget()->GetNativeView();
  // Disable the system shadow. BubbleFrameView will draw a custom shadow.
  init_params.shadow_type = views::Widget::InitParams::ShadowType::kNone;
  init_params.remove_standard_frame = true;
  // `autosize` is not compatible with TabDialogManager's `animated`.
  init_params.autosize = false;
  init_params.name = "PasswordChangeToast";
  widget->Init(std::move(init_params));

  auto tab_dialog_params = std::make_unique<tabs::TabDialogManager::Params>();
  tab_dialog_params->close_on_navigate = false;
  tab_dialog_params->close_on_detach = false;
  tab_dialog_params->disable_input = false;
  tab_dialog_params->animated = true;

  tab_dialog_manager->ShowDialog(widget.get(), std::move(tab_dialog_params));

  toast_widget_ = std::move(widget);
  toast_widget_->MakeCloseSynchronous(
      base::BindOnce(&PasswordChangeUIController::CloseToastWidget,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PasswordChangeUIController::ShowDialog(
    std::unique_ptr<ui::DialogModel> dialog_model) {
  CHECK(tab_interface_);
  if (!tab_interface_->CanShowModalUI()) {
    return;
  }

  views::BubbleDialogModelHost* model_host =
      views::BubbleDialogModelHost::CreateModal(std::move(dialog_model),
                                                ui::mojom::ModalType::kChild)
          .release();
  // TODO(crbug.com/338254375): Remove once it is a default state.
  model_host->SetOwnershipOfNewWidget(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET);

  auto tab_dialog_params = std::make_unique<tabs::TabDialogManager::Params>();
  tab_dialog_params->close_on_navigate = false;
  dialog_widget_ =
      tab_interface_->GetTabFeatures()
          ->tab_dialog_manager()
          ->CreateAndShowDialog(model_host, std::move(tab_dialog_params));
  dialog_widget_->MakeCloseSynchronous(
      base::BindOnce(&PasswordChangeUIController::CloseDialogWidget,
                     weak_ptr_factory_.GetWeakPtr()));

  views::View* focused_view = model_host->GetInitiallyFocusedView();
  if (focused_view) {
    focused_view->RequestFocus();
  }
}

void PasswordChangeUIController::OnToastCanceled() {
  CHECK(password_change_delegate_);
  LogToastEvent(state_, PasswordChangeToastEvent::kCanceled);
  if (state_ == PasswordChangeDelegate::State::kPasswordSuccessfullyChanged ||
      state_ == PasswordChangeDelegate::State::kCanceled) {
    // Successful and canceled states are terminal, there is no more UI to show.
    password_change_delegate_->Stop();
    return;
  }
  password_change_delegate_->CancelPasswordChangeFlow();
}

void PasswordChangeUIController::OnDialogCanceled() {
  CHECK(password_change_delegate_);
  LogDialogAction(state_, PasswordChangeDialogAction::kCancelButtonClicked);
  if (state_ == PasswordChangeDelegate::State::kWaitingForAgreement ||
      state_ == PasswordChangeDelegate::State::kOfferingPasswordChange ||
      state_ == PasswordChangeDelegate::State::kLoginFormDetected) {
    password_change_delegate_->OnPasswordChangeDeclined();
  }
  password_change_delegate_->Stop();
}

void PasswordChangeUIController::OpenPasswordChangeTab() {
  CHECK(password_change_delegate_);
  password_change_delegate_->OpenPasswordChangeTab();
  password_change_delegate_->Stop();
}

void PasswordChangeUIController::OpenPasswordChangeTabFromDialog() {
  LogDialogAction(state_, PasswordChangeDialogAction::kAcceptButtonClicked);
  OpenPasswordChangeTab();
}

void PasswordChangeUIController::OpenPasswordChangeTabFromToast() {
  LogToastEvent(state_, PasswordChangeToastEvent::kOpenPasswordChangeTab);
  OpenPasswordChangeTab();
}

void PasswordChangeUIController::StartPasswordChangeFlow() {
  CHECK(password_change_delegate_);
  LogDialogAction(state_, PasswordChangeDialogAction::kAcceptButtonClicked);
  password_change_delegate_->StartPasswordChangeFlow();
}

void PasswordChangeUIController::OnPrivacyNoticeAccepted() {
  CHECK(password_change_delegate_);
  LogDialogAction(state_, PasswordChangeDialogAction::kAcceptButtonClicked);
  password_change_delegate_->OnPrivacyNoticeAccepted();
}

void PasswordChangeUIController::ShowPasswordDetails() {
  CHECK(password_change_delegate_);
  password_change_delegate_->OpenPasswordDetails();
  password_change_delegate_->Stop();
}

void PasswordChangeUIController::NavigateToPasswordChangeSettings() {
  LogDialogAction(state_, PasswordChangeDialogAction::kLinkClicked);
  ShowSingletonTabOverwritingNTP(
      Profile::FromBrowserContext(
          tab_interface_->GetContents()->GetBrowserContext()),
      GURL(chrome::kChromeUiPasswordChangeUrl),
      NavigateParams::IGNORE_AND_NAVIGATE);
}

void PasswordChangeUIController::RetryLoginCheck() {
  CHECK(password_change_delegate_);
  LogToastEvent(PasswordChangeDelegate::State::kLoginFormDetected,
                PasswordChangeToastEvent::kRetry);
  password_change_delegate_->RetryLoginCheck();
}

void PasswordChangeUIController::CloseDialogWidget(
    views::Widget::ClosedReason reason) {
  dialog_widget_.reset();
}

void PasswordChangeUIController::CloseToastWidget(
    views::Widget::ClosedReason reason) {
  toast_view_ = nullptr;
  toast_widget_.reset();
  toast_delegate_.reset();
}
