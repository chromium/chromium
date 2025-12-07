// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/auto_reset.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_icon_name_and_origin_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_dialog_delegate.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/ui/web_applications/web_app_info_image_source.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/installable/ml_install_operation_tracker.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
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
bool g_auto_decline_pwa_for_testing = false;
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
    PwaInProductHelpState iph_state,
    bool show_initiating_origin) {
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

  DialogImageInfo dialog_image_info =
      web_app_info->GetIconBitmapsForSecureSurfaces();
  gfx::ImageSkia icon_image(
      std::make_unique<WebAppInfoImageSource>(
          kIconSize, std::move(dialog_image_info.bitmaps)),
      gfx::Size(kIconSize, kIconSize));
  auto app_name = web_app_info->title;
  GURL start_url = web_app_info->start_url();

  auto delegate = std::make_unique<web_app::WebAppInstallDialogDelegate>(
      web_contents, std::move(web_app_info), std::move(install_tracker),
      std::move(callback), std::move(iph_state), prefs, tracker,
      InstallDialogType::kSimple);
  auto delegate_weak_ptr = delegate->AsWeakPtr();

  auto dialog_model_builder = ui::DialogModel::Builder(std::move(delegate));
  dialog_model_builder.SetInternalName("WebAppSimpleInstallDialog")
      .SetTitle(l10n_util::GetStringUTF16(IDS_INSTALL_PWA_DIALOG_TITLE))
      .AddOkButton(base::BindOnce(&WebAppInstallDialogDelegate::OnAccept,
                                  delegate_weak_ptr),
                   ui::DialogModel::Button::Params().SetLabel(
                       l10n_util::GetStringUTF16(IDS_INSTALL)))
      .AddCancelButton(base::BindOnce(&WebAppInstallDialogDelegate::OnCancel,
                                      delegate_weak_ptr))
      .SetCloseActionCallback(base::BindOnce(
          &WebAppInstallDialogDelegate::OnClose, delegate_weak_ptr))
      .SetDialogDestroyingCallback(base::BindOnce(
          &WebAppInstallDialogDelegate::OnDestroyed, delegate_weak_ptr))
      .OverrideDefaultButton(ui::mojom::DialogButton::kCancel)
      .AddCustomField(
          std::make_unique<views::BubbleDialogModelHost::CustomView>(
              WebAppIconNameAndOriginView::Create(
                  icon_image, app_name.value(), start_url,
                  dialog_image_info.is_maskable),
              views::BubbleDialogModelHost::FieldType::kControl));
  // Only show the initiating origin subtitle label for background document
  // installs from the Web Install API.
  if (show_initiating_origin) {
    url::Origin initiating_origin =
        web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin();
    std::u16string origin_url = url_formatter::FormatOriginForSecurityDisplay(
        initiating_origin, url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
    dialog_model_builder.SetSubtitle(l10n_util::GetStringFUTF16(
        IDS_INSTALL_PWA_DIALOG_ORIGIN_LABEL, origin_url));
  }
  auto dialog_model = dialog_model_builder.Build();
  auto dialog = views::BubbleDialogModelHost::CreateModal(
      std::move(dialog_model), ui::mojom::ModalType::kChild);

  if (g_dont_close_on_deactivate) {
    dialog->set_close_on_deactivate(false);
  }
  dialog_delegate = dialog->AsBubbleDialogDelegate();
  views::Widget* simple_dialog_widget =
      constrained_window::ShowWebModalDialogViews(dialog.release(),
                                                  web_contents);
  if (IsWidgetCurrentSizeSmallerThanPreferredSize(simple_dialog_widget)) {
    delegate_weak_ptr->CloseDialogAsIgnored();
    return;
  }
  delegate_weak_ptr->OnWidgetShownStartTracking(simple_dialog_widget);

  base::RecordAction(base::UserMetricsAction("WebAppInstallShown"));
  if (g_auto_accept_pwa_for_testing) {
    dialog_delegate->AcceptDialog();
  }
  if (g_auto_decline_pwa_for_testing) {
    dialog_delegate->CancelDialog();
  }
}

base::AutoReset<bool> SetAutoAcceptPWAInstallConfirmationForTesting() {
  return base::AutoReset<bool>(&g_auto_accept_pwa_for_testing, true);
}

base::AutoReset<bool> SetAutoDeclinePWAInstallConfirmationForTesting() {
  return base::AutoReset<bool>(&g_auto_decline_pwa_for_testing, true);
}

base::AutoReset<bool> SetDontCloseOnDeactivateForTesting() {
  return base::AutoReset<bool>(&g_dont_close_on_deactivate, true);
}

}  // namespace web_app
