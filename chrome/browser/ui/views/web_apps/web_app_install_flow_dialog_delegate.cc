// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_install_flow_dialog_delegate.h"

#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/url_identity.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/controls/site_icon_text_and_origin_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/web_apps/progress_delay.h"
#include "chrome/browser/ui/views/web_apps/web_app_icon_name_and_origin_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_dialog_delegate.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_dialog_flow_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_intro_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_options_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_progress_view.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/ui/web_applications/web_app_info_image_source.h"
#include "chrome/browser/web_applications/model/dialog_image_info.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_screenshot_fetcher.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "components/webapps/browser/installable/ml_install_operation_tracker.h"
#include "components/webapps/common/constants.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace web_app {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPwaInstallDialogCancelButtonId);

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(WebAppInstallFlowDialogDelegate,
                                      kInstallDialogFlowViewId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(WebAppInstallFlowDialogDelegate,
                                      kLearnMoreButtonId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(WebAppInstallFlowDialogDelegate,
                                      kCancelButtonId);

std::ostream& operator<<(std::ostream& os, InstallOsType type) {
  switch (type) {
    case InstallOsType::kWin:
      return os << "kWin";
    case InstallOsType::kMac:
      return os << "kMac";
    case InstallOsType::kCros:
      return os << "kCros";
    case InstallOsType::kOther:
      return os << "kOther";
  }
  return os << "Unknown";
}

WebAppInstallFlowDialogDelegate::WebAppInstallFlowDialogDelegate(
    content::WebContents* web_contents,
    std::unique_ptr<WebAppInstallInfo> install_info,
    std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker,
    AppInstallationAcceptanceCallback callback,
    PwaInProductHelpState iph_state,
    PrefService* prefs,
    feature_engagement::Tracker* tracker,
    InstallDialogType dialog_type,
    InstallOsType os_type,
    std::unique_ptr<ProgressDelay> progress_delay)
    : WebAppInstallDialogDelegate(web_contents,
                                  std::move(install_info),
                                  std::move(install_tracker),
                                  std::move(callback),
                                  iph_state,
                                  prefs,
                                  tracker,
                                  dialog_type),
      os_type_(os_type),
      progress_delay_(std::move(progress_delay)) {
  CHECK(progress_delay_);
}

WebAppInstallFlowDialogDelegate::~WebAppInstallFlowDialogDelegate() = default;

bool WebAppInstallFlowDialogDelegate::OnOkButtonClicked() {
  if (!dialog_model()) {
    return false;
  }

  // TODO(crbug.com/380497638): Trigger the installation earlier in the flow.
  if (current_step_ == InstallDialogStep::kSuccessful) {
    // TODO(b/492657179): Implement the logic to open the newly installed
    // app in a tab/window.
    OnAccept();
    return true;
  }

  // Update install dialog step.
  switch (current_step_) {
    case InstallDialogStep::kInstallDialog:
      if (os_type_ == InstallOsType::kOther) {
        current_step_ = InstallDialogStep::kProgress;
      } else {
        current_step_ = InstallDialogStep::kInstallerOptions;
      }
      break;

    case InstallDialogStep::kInstallerOptions:
      current_step_ = InstallDialogStep::kProgress;
      break;

    case InstallDialogStep::kProgress:
      current_step_ = InstallDialogStep::kSuccessful;
      break;

    case InstallDialogStep::kSuccessful:
      NOTREACHED();
  }

  // Actions based on the new current_step_
  if (current_step_ == InstallDialogStep::kProgress) {
    // Start progress delay.
    progress_delay_->Start(
        base::BindRepeating(&WebAppInstallFlowDialogDelegate::OnProgress,
                            weak_ptr_factory_.GetWeakPtr()));

    // Hide buttons on progress step.
    dialog_model()->SetVisible(kLearnMoreButtonId, false);
    dialog_model()->SetVisible(kPwaInstallDialogInstallButton, false);
    dialog_model()->SetVisible(kPwaInstallDialogCancelButtonId, false);
  } else if (current_step_ == InstallDialogStep::kSuccessful) {
    dialog_model()->SetVisible(kPwaInstallDialogInstallButton, true);
    dialog_model()->SetVisible(kPwaInstallDialogCancelButtonId, true);
  }

  if (flow_view_) {
    flow_view_->UpdateStepVisibility(current_step_);
  }

  UpdateDialogTitle(current_step_);

  // Last dialog to show the install button.
  // TODO(crbug.com/380497638): Trigger the installation earlier in the flow.
  if (current_step_ == InstallDialogStep::kSuccessful) {
    ui::DialogModel::Button* ok_button =
        dialog_model()->GetButtonByUniqueId(kPwaInstallDialogInstallButton);
    if (ok_button) {
      dialog_model()->SetButtonLabel(
          ok_button,
          l10n_util::GetStringUTF16(IDS_WEB_APP_INSTALL_SUCCESS_OPEN_APP));
    }
    ui::DialogModel::Button* cancel_button =
        dialog_model()->GetButtonByUniqueId(kPwaInstallDialogCancelButtonId);
    if (cancel_button) {
      dialog_model()->SetButtonLabel(cancel_button,
                                     l10n_util::GetStringUTF16(IDS_CLOSE));
    }
  }

  return false;
}

void WebAppInstallFlowDialogDelegate::OnLearnMoreButtonClicked() {
  web_contents()->OpenURL(
      content::OpenURLParams(
          GURL(chrome::kInstallDialogFlowLearnMoreURL), content::Referrer(),
          WindowOpenDisposition::NEW_FOREGROUND_TAB, ui::PAGE_TRANSITION_LINK,
          /*is_renderer_initiated=*/false),
      base::DoNothing());
}

// Updates dialog title based on current step.
void WebAppInstallFlowDialogDelegate::UpdateDialogTitle(
    InstallDialogStep step) {
  std::u16string title;
  switch (step) {
    case InstallDialogStep::kInstallDialog:
      NOTREACHED();
    case InstallDialogStep::kInstallerOptions:
      title = l10n_util::GetStringUTF16(dialog_type() == InstallDialogType::kDiy
                                            ? IDS_DIY_APP_INSTALL_DIALOG_TITLE
                                            : IDS_INSTALL_PWA_DIALOG_TITLE);
      break;
    case InstallDialogStep::kProgress:
      title = l10n_util::GetStringUTF16(IDS_INSTALL_PWA_DIALOG_INSTALLING);
      break;
    case InstallDialogStep::kSuccessful:
      title = l10n_util::GetStringUTF16(IDS_WEB_APP_INSTALL_SUCCESS_TITLE);
      break;
  }

  if (dialog_model() && dialog_model()->host()) {
    auto* host =
        static_cast<views::BubbleDialogModelHost*>(dialog_model()->host());
    host->SetTitle(title);
    host->SetAccessibleTitle(title);
    // Clear the subtitle for all subsequent steps.
    host->SetSubtitle(std::u16string());
  }
}

void WebAppInstallFlowDialogDelegate::OnProgress(
    std::optional<double> percent) {
  if (percent.has_value()) {
    if (progress_view_) {
      progress_view_->SetProgressValue(percent.value());
    }
  } else {
    if (dialog_model()) {
      dialog_model()->SetVisible(kPwaInstallDialogInstallButton, true);
      dialog_model()->SetButtonLabel(
          dialog_model()->GetButtonByUniqueId(kPwaInstallDialogInstallButton),
          u"Next");
    }
  }
}

// Builds and shows an install dialog flow according to the install_type.
void WebAppInstallFlowDialogDelegate::Show(
    content::WebContents* web_contents,
    std::unique_ptr<WebAppInstallInfo> install_info,
    std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker,
    AppInstallationAcceptanceCallback callback,
    PwaInProductHelpState iph_state,
    base::WeakPtr<WebAppScreenshotFetcher> screenshot_fetcher,
    bool show_initiating_origin,
    InstallDialogType install_type,
    InstallOsType os_type,
    std::unique_ptr<ProgressDelay> progress_delay) {
  auto* browser_context = web_contents->GetBrowserContext();
  Profile* profile = Profile::FromBrowserContext(browser_context);
  PrefService* prefs = profile->GetPrefs();
  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForBrowserContext(browser_context);

  DialogImageInfo dialog_image_info =
      install_info->GetIconBitmapsForSecureSurfaces();
  gfx::ImageSkia icon_image(
      std::make_unique<WebAppInfoImageSource>(
          kIconSize, std::move(dialog_image_info.bitmaps)),
      gfx::Size(kIconSize, kIconSize));

  std::u16string title = install_info->title.value();
  GURL start_url = install_info->start_url();
  auto delegate = std::make_unique<WebAppInstallFlowDialogDelegate>(
      web_contents, std::move(install_info), std::move(install_tracker),
      std::move(callback), std::move(iph_state), prefs, tracker, install_type,
      os_type, std::move(progress_delay));
  auto delegate_weak_ptr = delegate->AsWeakPtr();

  absl::flat_hash_map<InstallDialogStep, std::unique_ptr<views::View>>
      install_step_to_view;

  // kInstallDialog
  install_step_to_view[InstallDialogStep::kInstallDialog] =
      WebAppInstallIntroView::Create(
          install_type, icon_image, title, start_url,
          dialog_image_info.is_maskable,
          base::BindRepeating(
              &WebAppInstallDialogDelegate::OnTextFieldChangedMaybeUpdateButton,
              delegate_weak_ptr));

  // kInstallerOptions
  install_step_to_view[InstallDialogStep::kInstallerOptions] =
      std::make_unique<WebAppInstallOptionsView>(os_type, title, icon_image);

  // kProgress
  auto progress_view = std::make_unique<WebAppInstallProgressView>();
  auto progress_view_weak_ptr = progress_view->GetWeakPtr();
  install_step_to_view[InstallDialogStep::kProgress] = std::move(progress_view);

  // kSuccessful
  auto successful_view =
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .Build();
  successful_view->AddChildView(WebAppIconNameAndOriginView::Create(
      icon_image, title, start_url, dialog_image_info.is_maskable));

  install_step_to_view[InstallDialogStep::kSuccessful] =
      std::move(successful_view);

  auto flow_view =
      std::make_unique<WebAppInstallFlowView>(std::move(install_step_to_view));
  flow_view->SetProperty(views::kElementIdentifierKey,
                         kInstallDialogFlowViewId);
  auto flow_view_weak_ptr = flow_view->GetWeakPtr();
  delegate->SetFlowView(flow_view_weak_ptr);
  delegate->SetProgressView(progress_view_weak_ptr);

  views::View* focusable_view =
      flow_view->GetViewForStep(InstallDialogStep::kInstallDialog);

  auto dialog_model_builder = ui::DialogModel::Builder(std::move(delegate));
  dialog_model_builder.SetInternalName("WebAppInstallFlowDialog")
      .SetAccessibleTitle(
          l10n_util::GetStringUTF16(install_type == InstallDialogType::kDiy
                                        ? IDS_DIY_APP_INSTALL_DIALOG_TITLE
                                        : IDS_INSTALL_PWA_DIALOG_TITLE))
      .SetTitle(
          l10n_util::GetStringUTF16(install_type == InstallDialogType::kDiy
                                        ? IDS_DIY_APP_INSTALL_DIALOG_TITLE
                                        : IDS_INSTALL_PWA_DIALOG_TITLE))
      // TODO(b/473080055): Use a translated string. Should use the correct
      // subtitle if DIY vs simple and detailed like the title above.
      .SetSubtitle(u"Access this site on a dedicated window on your computer")
      .AddExtraButton(
          base::BindRepeating(
              [](base::WeakPtr<WebAppInstallFlowDialogDelegate> delegate,
                 const ui::Event&) {
                if (delegate) {
                  delegate->OnLearnMoreButtonClicked();
                }
              },
              delegate_weak_ptr),
          ui::DialogModel::Button::Params()
              .SetLabel(
                  l10n_util::GetStringUTF16(IDS_LEARN_MORE_MAYBE_TITLE_CASE))
              .SetId(kLearnMoreButtonId))
      .AddOkButton(
          base::BindRepeating(
              [](base::WeakPtr<WebAppInstallFlowDialogDelegate> delegate) {
                return delegate ? delegate->OnOkButtonClicked() : true;
              },
              delegate_weak_ptr),
          // TODO(crbug.com/503767931): Localize this text.
          ui::DialogModel::Button::Params().SetLabel(u"Next").SetId(
              WebAppInstallDialogDelegate::kPwaInstallDialogInstallButton))
      .AddCancelButton(base::BindOnce(&WebAppInstallDialogDelegate::OnCancel,
                                      delegate_weak_ptr),
                       ui::DialogModel::Button::Params().SetId(
                           kPwaInstallDialogCancelButtonId))
      .SetCloseActionCallback(base::BindOnce(
          &WebAppInstallDialogDelegate::OnClose, delegate_weak_ptr))
      .SetDialogDestroyingCallback(base::BindOnce(
          &WebAppInstallDialogDelegate::OnDestroyed, delegate_weak_ptr))
      .OverrideDefaultButton(ui::mojom::DialogButton::kCancel)
      .AddCustomField(
          std::make_unique<views::BubbleDialogModelHost::CustomView>(
              std::move(flow_view),
              views::BubbleDialogModelHost::FieldType::kControl,
              focusable_view),
          WebAppInstallFlowDialogDelegate::kInstallDialogFlowViewId);

  if (install_type == InstallDialogType::kDiy) {
    dialog_model_builder.SetInitiallyFocusedField(kInstallDialogFlowViewId);
  }

  if (show_initiating_origin) {
    url::Origin initiating_origin =
        web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin();
    std::u16string origin_url = url_formatter::FormatOriginForSecurityDisplay(
        initiating_origin, url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
    dialog_model_builder.SetSubtitle(l10n_util::GetStringFUTF16(
        IDS_INSTALL_PWA_DIALOG_ORIGIN_LABEL, origin_url));
  }

  auto dialog = views::BubbleDialogModelHost::CreateModal(
      dialog_model_builder.Build(), ui::mojom::ModalType::kChild);

  views::Widget* widget = constrained_window::ShowWebModalDialogViews(
      dialog.release(), web_contents);

  if (IsWidgetCurrentSizeSmallerThanPreferredSize(widget)) {
    delegate_weak_ptr->CloseDialogAsIgnored();
    return;
  }
  delegate_weak_ptr->OnWidgetShownStartTracking(widget);
}

}  // namespace web_app
