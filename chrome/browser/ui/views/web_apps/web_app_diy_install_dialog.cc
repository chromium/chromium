// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/functional/callback_helpers.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/url_identity.h"
#include "chrome/browser/ui/views/controls/site_icon_text_and_origin_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_icon_name_and_origin_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_info_image_source.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_dialog_delegate.h"
#include "chrome/browser/ui/views/web_apps/web_app_views_utils.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/strings/grit/components_strings.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/webapps/browser/installable/ml_install_operation_tracker.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
// TODO(crbug.com/40147906): Enable gn check once it learns about conditional
// includes.
#include "components/metrics/structured/structured_events.h"  // nogncheck
#include "components/metrics/structured/structured_metrics_client.h"  // nogncheck
#endif

namespace {

bool g_auto_accept_diy_dialog_for_testing = false;

#if BUILDFLAG(IS_CHROMEOS)
namespace cros_events = metrics::structured::events::v2::cr_os_events;
#endif

}  // namespace

namespace web_app {

void ShowDiyAppInstallDialog(
    content::WebContents* web_contents,
    std::unique_ptr<web_app::WebAppInstallInfo> web_app_info,
    std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker,
    AppInstallationAcceptanceCallback callback,
    PwaInProductHelpState iph_state) {
  CHECK(web_app_info->is_diy_app);
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
  Profile* profile = Profile::FromBrowserContext(browser_context);
  PrefService* prefs = profile->GetPrefs();

#if BUILDFLAG(IS_CHROMEOS)
  webapps::AppId app_id =
      web_app::GenerateAppIdFromManifestId(web_app_info->manifest_id());
  metrics::structured::StructuredMetricsClient::Record(
      cros_events::AppDiscovery_Browser_AppInstallDialogShown().SetAppId(
          app_id));
#endif  // BUILDFLAG(IS_CHROMEOS)

  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForBrowserContext(browser_context);

  gfx::ImageSkia icon_image(std::make_unique<WebAppInfoImageSource>(
                                kIconSize, web_app_info->icon_bitmaps.any),
                            gfx::Size(kIconSize, kIconSize));
  GURL start_url = web_app_info->start_url();

  // Fallback to using the document title if the web_app_info->title is not
  // populated, as the document title is always guaranteed to exist.
  std::u16string app_name = web_app_info->title;
  if (app_name.empty()) {
    app_name = UrlIdentity::CreateFromUrl(profile, start_url,
                                          {UrlIdentity::Type::kDefault}, {})
                   .name;
  }

  auto delegate = std::make_unique<web_app::WebAppInstallDialogDelegate>(
      web_contents, std::move(web_app_info), std::move(install_tracker),
      std::move(callback), std::move(iph_state), prefs, tracker,
      InstallDialogType::kDiy);
  auto delegate_weak_ptr = delegate->AsWeakPtr();

  auto dialog_model =
      ui::DialogModel::Builder(std::move(delegate))
          .SetInternalName("WebAppDiyInstallDialog")
          .SetTitle(l10n_util::GetStringUTF16(IDS_DIY_APP_INSTALL_DIALOG_TITLE))
          .SetSubtitle(
              l10n_util::GetStringUTF16(IDS_DIY_APP_INSTALL_DIALOG_SUBTITLE))
          .AddOkButton(
              base::BindOnce(&WebAppInstallDialogDelegate::OnAccept,
                             delegate_weak_ptr),
              ui::DialogModel::Button::Params()
                  .SetLabel(l10n_util::GetStringUTF16(IDS_INSTALL))
                  .SetId(WebAppInstallDialogDelegate::kDiyAppsDialogOkButtonId))
          .AddCancelButton(base::BindOnce(
              &WebAppInstallDialogDelegate::OnCancel, delegate_weak_ptr))
          .SetCloseActionCallback(base::BindOnce(
              &WebAppInstallDialogDelegate::OnClose, delegate_weak_ptr))
          .SetDialogDestroyingCallback(base::BindOnce(
              &WebAppInstallDialogDelegate::OnDestroyed, delegate_weak_ptr))
          .OverrideDefaultButton(ui::mojom::DialogButton::kCancel)
          .Build();

  dialog_model->AddCustomField(std::make_unique<
                               views::BubbleDialogModelHost::CustomView>(
      std::make_unique<SiteIconTextAndOriginView>(
          icon_image, app_name,
          l10n_util::GetStringUTF16(IDS_DIY_APP_AX_BUBBLE_NAME_LABEL),
          start_url, web_contents,
          base::BindRepeating(
              &WebAppInstallDialogDelegate::OnTextFieldChangedMaybeUpdateButton,
              delegate_weak_ptr)),
      views::BubbleDialogModelHost::FieldType::kControl));

  auto dialog = views::BubbleDialogModelHost::CreateModal(
      std::move(dialog_model), ui::mojom::ModalType::kChild);

  views::BubbleDialogDelegate* dialog_delegate =
      dialog->AsBubbleDialogDelegate();
  views::Widget* diy_dialog_widget =
      constrained_window::ShowWebModalDialogViews(dialog.release(),
                                                  web_contents);
  delegate_weak_ptr->StartObservingForPictureInPictureOcclusion(
      diy_dialog_widget);

  base::RecordAction(base::UserMetricsAction("WebAppDiyInstallShown"));

  if (g_auto_accept_diy_dialog_for_testing) {
    dialog_delegate->AcceptDialog();
  }
}

void SetAutoAcceptDiyAppsInstallDialogForTesting(bool auto_accept) {
  g_auto_accept_diy_dialog_for_testing = auto_accept;
}

}  // namespace web_app
