// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_install_flow_dialog_delegate.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/url_identity.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/controls/site_icon_text_and_origin_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_icon_name_and_origin_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_dialog_delegate.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_dialog_flow_view.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/ui/web_applications/web_app_info_image_source.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_screenshot_fetcher.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "components/webapps/browser/installable/ml_install_operation_tracker.h"
#include "components/webapps/common/constants.h"
#include "content/public/browser/web_contents.h"
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
#include "ui/views/widget/widget.h"
#include "url/origin.h"

namespace web_app {

WebAppInstallFlowDialogDelegate::WebAppInstallFlowDialogDelegate(
    content::WebContents* web_contents,
    std::unique_ptr<WebAppInstallInfo> install_info,
    std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker,
    AppInstallationAcceptanceCallback callback,
    PwaInProductHelpState iph_state,
    PrefService* prefs,
    feature_engagement::Tracker* tracker,
    InstallDialogType dialog_type)
    : WebAppInstallDialogDelegate(web_contents,
                                  std::move(install_info),
                                  std::move(install_tracker),
                                  std::move(callback),
                                  iph_state,
                                  prefs,
                                  tracker,
                                  dialog_type) {}

WebAppInstallFlowDialogDelegate::~WebAppInstallFlowDialogDelegate() = default;

bool WebAppInstallFlowDialogDelegate::OnOkButtonClicked() {
  if (current_step_ == InstallDialogStep::kSuccessful) {
    OnAccept();
    return true;
  }

  // Update install dialog step.
  if (current_step_ == InstallDialogStep::kInstallDialog) {
    current_step_ = InstallDialogStep::kInstallerOptions;
  } else if (current_step_ == InstallDialogStep::kInstallerOptions) {
    current_step_ = InstallDialogStep::kProgress;
  } else if (current_step_ == InstallDialogStep::kProgress) {
    current_step_ = InstallDialogStep::kSuccessful;
  }

  if (flow_view_) {
    flow_view_->UpdateStepVisibility(current_step_);
  }

  // Last dialog to show the install button.
  // TODO(crbug.com/380497638): Trigger the installation earlier in the flow.
  if (current_step_ == InstallDialogStep::kSuccessful && dialog_model()) {
    ui::DialogModel::Button* ok_button =
        dialog_model()->GetButtonByUniqueId(kPwaInstallDialogInstallButton);
    if (ok_button) {
      dialog_model()->SetButtonLabel(ok_button,
                                     l10n_util::GetStringUTF16(IDS_DONE));
    }
  }

  return false;
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
    InstallDialogType install_type) {
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

  auto flow_view = std::make_unique<WebAppInstallFlowView>(
      icon_image, title, start_url, dialog_image_info.is_maskable);
  auto flow_view_weak_ptr = flow_view->GetWeakPtr();

  auto install_info_description = install_info->description.value();

  auto delegate = std::make_unique<WebAppInstallFlowDialogDelegate>(
      web_contents, std::move(install_info), std::move(install_tracker),
      std::move(callback), std::move(iph_state), prefs, tracker, install_type);
  auto* delegate_ptr = delegate.get();
  auto delegate_weak_ptr = delegate_ptr->AsWeakPtr();

  delegate_ptr->SetFlowView(flow_view_weak_ptr);

  views::View* focusable_view = nullptr;
  std::unique_ptr<views::View> step_view;

  switch (install_type) {
    case InstallDialogType::kDetailed: {
      const std::u16string description = gfx::TruncateString(
          install_info_description, webapps::kMaximumDescriptionLength,
          gfx::CHARACTER_BREAK);
      step_view = CreateDetailedInstallDialogView(
          icon_image, title, start_url, dialog_image_info.is_maskable,
          std::move(screenshot_fetcher), description);
      break;
    }
    case InstallDialogType::kDiy: {
      if (title.empty()) {
        title = UrlIdentity::CreateFromUrl(profile, start_url,
                                           {UrlIdentity::Type::kDefault}, {})
                    .name;
      }
      step_view = CreateDiyInstallDialogView(
          icon_image, title, start_url, web_contents,
          base::BindRepeating(
              &WebAppInstallDialogDelegate::OnTextFieldChangedMaybeUpdateButton,
              delegate_weak_ptr));
      focusable_view = static_cast<SiteIconTextAndOriginView*>(step_view.get())
                           ->title_field();
      break;
    }
    case InstallDialogType::kSimple:
      step_view = CreateSimpleInstallDialogView(icon_image, title, start_url,
                                                dialog_image_info.is_maskable);
      break;
  }

  flow_view->SetStepView(InstallDialogStep::kInstallDialog,
                         std::move(step_view));

  auto dialog_model_builder = ui::DialogModel::Builder(std::move(delegate));
  dialog_model_builder.SetInternalName("WebAppInstallFlowDialog")
      .SetTitle(
          l10n_util::GetStringUTF16(install_type == InstallDialogType::kDiy
                                        ? IDS_DIY_APP_INSTALL_DIALOG_TITLE
                                        : IDS_INSTALL_PWA_DIALOG_TITLE))
      .AddOkButton(
          base::BindRepeating(
              [](base::WeakPtr<WebAppInstallFlowDialogDelegate> delegate) {
                return delegate ? delegate->OnOkButtonClicked() : true;
              },
              delegate_weak_ptr),
          ui::DialogModel::Button::Params().SetLabel(u"Next").SetId(
              WebAppInstallDialogDelegate::kPwaInstallDialogInstallButton))
      .AddCancelButton(base::BindOnce(&WebAppInstallDialogDelegate::OnCancel,
                                      delegate_weak_ptr))
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
          WebAppInstallDialogDelegate::kDiyAppsDialogInputTextId);

  if (install_type == InstallDialogType::kDiy) {
    dialog_model_builder.SetSubtitle(
        l10n_util::GetStringUTF16(IDS_DIY_APP_INSTALL_DIALOG_SUBTITLE));
    dialog_model_builder.SetInitiallyFocusedField(
        WebAppInstallDialogDelegate::kDiyAppsDialogInputTextId);
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
