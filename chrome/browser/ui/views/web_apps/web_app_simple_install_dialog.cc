// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/metrics/user_metrics.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/web_apps/pwa_confirmation_bubble_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_icon_name_and_origin_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_info_image_source.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_dialog_delegate.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/webapps/browser/installable/ml_install_operation_tracker.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/web_applications/web_app_helpers.h"
// TODO(crbug.com/40147906): Enable gn check once it learns about conditional
// includes.
#include "components/metrics/structured/structured_events.h"  // nogncheck
#include "components/metrics/structured/structured_metrics_client.h"  // nogncheck
#endif

namespace web_app {

namespace {
bool g_auto_accept_pwa_for_testing = false;
bool g_dont_close_on_deactivate = false;

#if BUILDFLAG(IS_CHROMEOS)
namespace cros_events = metrics::structured::events::v2::cr_os_events;
#endif

}  // namespace

void ShowSimpleInstallDialogForWebApps(
    content::WebContents* web_contents,
    std::unique_ptr<web_app::WebAppInstallInfo> web_app_info,
    std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker,
    AppInstallationAcceptanceCallback callback,
    PwaInProductHelpState iph_state) {
  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  if (!browser) {
    std::move(callback).Run(false, nullptr);
    return;
  }

  // Do not show the dialog if it is already being shown.
  const web_modal::WebContentsModalDialogManager* manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents);
  if (!manager || manager->IsDialogActive()) {
    std::move(callback).Run(/*is_accepted=*/false, nullptr);
    return;
  }

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  views::View* anchor_view =
      browser_view->toolbar_button_provider()->GetAnchorView(
          PageActionIconType::kPwaInstall);
  PageActionIconView* icon =
      browser_view->toolbar_button_provider()->GetPageActionIconView(
          PageActionIconType::kPwaInstall);
  auto* browser_context = web_contents->GetBrowserContext();
  PrefService* prefs = Profile::FromBrowserContext(browser_context)->GetPrefs();

#if BUILDFLAG(IS_CHROMEOS)
  webapps::AppId app_id =
      web_app::GenerateAppIdFromManifestId(web_app_info->manifest_id());
  metrics::structured::StructuredMetricsClient::Record(
      cros_events::AppDiscovery_Browser_AppInstallDialogShown().SetAppId(
          app_id));
#endif  // BUILDFLAG(IS_CHROMEOS)

  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForBrowserContext(browser_context);

  views::BubbleDialogDelegate* dialog_delegate = nullptr;

  if (base::FeatureList::IsEnabled(features::kWebAppUniversalInstall)) {
    gfx::ImageSkia icon_image(std::make_unique<WebAppInfoImageSource>(
                                  kIconSize, web_app_info->icon_bitmaps.any),
                              gfx::Size(kIconSize, kIconSize));
    auto app_name = web_app_info->title;
    GURL start_url = web_app_info->start_url();

    auto delegate = std::make_unique<web_app::WebAppInstallDialogDelegate>(
        web_contents, std::move(web_app_info), std::move(install_tracker),
        std::move(callback), std::move(iph_state), prefs, tracker,
        InstallDialogType::kSimple);
    auto delegate_weak_ptr = delegate->AsWeakPtr();

    auto dialog_model =
        ui::DialogModel::Builder(std::move(delegate))
            .SetInternalName("WebAppSimpleInstallDialog")
            .SetTitle(l10n_util::GetStringUTF16(IDS_INSTALL_PWA_DIALOG_TITLE))
            .AddOkButton(base::BindOnce(&WebAppInstallDialogDelegate::OnAccept,
                                        delegate_weak_ptr),
                         ui::DialogModel::Button::Params().SetLabel(
                             l10n_util::GetStringUTF16(IDS_INSTALL)))
            .AddCancelButton(base::BindOnce(
                &WebAppInstallDialogDelegate::OnCancel, delegate_weak_ptr))
            .SetCloseActionCallback(base::BindOnce(
                &WebAppInstallDialogDelegate::OnClose, delegate_weak_ptr))
            .SetDialogDestroyingCallback(base::BindOnce(
                &WebAppInstallDialogDelegate::OnDestroyed, delegate_weak_ptr))
            .OverrideDefaultButton(ui::mojom::DialogButton::kCancel)
            .AddCustomField(
                std::make_unique<views::BubbleDialogModelHost::CustomView>(
                    WebAppIconNameAndOriginView::Create(icon_image, app_name,
                                                        start_url),
                    views::BubbleDialogModelHost::FieldType::kControl))
            .Build();
    auto dialog = views::BubbleDialogModelHost::CreateModal(
        std::move(dialog_model), ui::mojom::ModalType::kChild);

    if (g_dont_close_on_deactivate) {
      dialog->set_close_on_deactivate(false);
    }
    dialog_delegate = dialog->AsBubbleDialogDelegate();
    if (icon) {
      dialog_delegate->SetAnchorView(icon);
    }
    views::Widget* modal_widget = constrained_window::ShowWebModalDialogViews(
        dialog.release(), web_contents);
    delegate_weak_ptr->StartObservingForPictureInPictureOcclusion(modal_widget);
  } else {
    auto* dialog_view = new PWAConfirmationBubbleView(
        anchor_view, web_contents->GetWeakPtr(), icon, std::move(web_app_info),
        std::move(install_tracker), std::move(callback), iph_state, prefs,
        tracker);
    if (g_dont_close_on_deactivate) {
      dialog_view->set_close_on_deactivate(false);
    }

    views::BubbleDialogDelegateView::CreateBubble(dialog_view)->Show();
    dialog_delegate = dialog_view->AsBubbleDialogDelegate();
    if (icon) {
      icon->Update();
      DCHECK(icon->GetVisible());
    }
  }

  base::RecordAction(base::UserMetricsAction("WebAppInstallShown"));
  if (g_auto_accept_pwa_for_testing) {
    dialog_delegate->AcceptDialog();
  }
}

void SetAutoAcceptPWAInstallConfirmationForTesting(bool auto_accept) {
  g_auto_accept_pwa_for_testing = auto_accept;
}

base::AutoReset<bool> SetDontCloseOnDeactivateForTesting() {
  return base::AutoReset<bool>(&g_dont_close_on_deactivate, true);
}

}  // namespace web_app
