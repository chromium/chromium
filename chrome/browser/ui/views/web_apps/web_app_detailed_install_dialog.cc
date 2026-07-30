// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <numeric>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/web_apps/image_carousel_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_icon_name_and_origin_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_dialog_delegate.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/ui/web_applications/web_app_info_image_source.h"
#include "chrome/browser/web_applications/model/dialog_image_info.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_screenshot_fetcher.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/webapps/browser/installable/installable_data.h"
#include "components/webapps/browser/installable/ml_install_operation_tracker.h"
#include "components/webapps/common/constants.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_CHROMEOS)
// TODO(crbug.com/40147906): Enable gn check once it learns about conditional
// includes.
#include "components/metrics/structured/structured_events.h"  // nogncheck
#include "components/metrics/structured/structured_metrics_client.h"  // nogncheck
#endif

namespace {

#if BUILDFLAG(IS_CHROMEOS)
namespace cros_events = metrics::structured::events::v2::cr_os_events;
#endif

}  // namespace

namespace web_app {

DEFINE_ELEMENT_IDENTIFIER_VALUE(kDetailedInstallDialogImageContainer);

// TODO(crbug.com/507106728): Delete this implementation once
// kWebAppInstallDialog is enabled by default and WebAppInstallIntroView
// takes over.

void ShowWebAppDetailedInstallDialog(
    content::WebContents* web_contents,
    std::unique_ptr<web_app::WebAppInstallInfo> install_info,
    std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker,
    AppInstallationAcceptanceCallback callback,
    base::WeakPtr<web_app::WebAppScreenshotFetcher> fetcher,
    PwaInProductHelpState iph_state) {
  // Do not show the dialog if it is already being shown.
  const web_modal::WebContentsModalDialogManager* manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents);
  if (!manager || manager->IsDialogActive()) {
    std::move(callback).Run(/*is_accepted=*/false, nullptr);
    return;
  }

  content::BrowserContext* browser_context = web_contents->GetBrowserContext();
  PrefService* const prefs =
      Profile::FromBrowserContext(browser_context)->GetPrefs();

  feature_engagement::Tracker* const tracker =
      feature_engagement::TrackerFactory::GetForBrowserContext(browser_context);

  DialogImageInfo dialog_image_info =
      install_info->GetIconBitmapsForSecureSurfaces();
  gfx::ImageSkia icon_image(
      std::make_unique<WebAppInfoImageSource>(
          kIconSize, std::move(dialog_image_info.bitmaps)),
      gfx::Size(kIconSize, kIconSize));

  auto title = install_info->title;
  GURL start_url = install_info->start_url();
  const std::u16string description = gfx::TruncateString(
      install_info->description.value(), webapps::kMaximumDescriptionLength,
      gfx::CHARACTER_BREAK);
  webapps::ManifestId manifest_id = install_info->manifest_id();

  auto delegate = std::make_unique<WebAppInstallDialogDelegate>(
      web_contents, std::move(install_info), std::move(install_tracker),
      std::move(callback), std::move(iph_state), prefs, tracker,
      InstallDialogType::kDetailed);
  auto delegate_weak_ptr = delegate->AsWeakPtr();

  std::unique_ptr<ui::DialogModel> dialog_model;
  dialog_model =
      ui::DialogModel::Builder(std::move(delegate))
          .SetInternalName("WebAppDetailedInstallDialog")
          .SetTitle(l10n_util::GetStringUTF16(IDS_INSTALL_PWA_DIALOG_TITLE))
          .AddCustomField(
              std::make_unique<views::BubbleDialogModelHost::CustomView>(
                  WebAppIconNameAndOriginView::Create(
                      icon_image, title.value(), start_url,
                      dialog_image_info.is_maskable),
                  views::BubbleDialogModelHost::FieldType::kControl))
          .AddParagraph(
              ui::DialogModelLabel(description).set_is_secondary(),
              l10n_util::GetStringUTF16(
                  IDS_WEB_APP_DETAILED_INSTALL_DIALOG_DESCRIPTION_TITLE))
          .AddOkButton(base::BindOnce(&WebAppInstallDialogDelegate::OnAccept,
                                      delegate_weak_ptr),
                       ui::DialogModel::Button::Params()
                           .SetLabel(l10n_util::GetStringUTF16(IDS_INSTALL))
                           .SetId(WebAppInstallDialogDelegate::
                                      kPwaInstallDialogInstallButton))
          .AddCancelButton(base::BindOnce(
              &WebAppInstallDialogDelegate::OnCancel, delegate_weak_ptr))
          .SetCloseActionCallback(base::BindOnce(
              &WebAppInstallDialogDelegate::OnClose, delegate_weak_ptr))
          .SetDialogDestroyingCallback(base::BindOnce(
              &WebAppInstallDialogDelegate::OnDestroyed, delegate_weak_ptr))
          .AddCustomField(
              std::make_unique<views::BubbleDialogModelHost::CustomView>(
                  std::make_unique<ImageCarouselView>(fetcher),
                  views::BubbleDialogModelHost::FieldType::kControl))
          .OverrideDefaultButton(ui::mojom::DialogButton::kCancel)
          .Build();
  auto dialog = views::BubbleDialogModelHost::CreateModal(
      std::move(dialog_model), ui::mojom::ModalType::kChild);
  views::Widget* detailed_dialog_widget =
      constrained_window::ShowWebModalDialogViews(dialog.release(),
                                                  web_contents);
  if (IsWidgetCurrentSizeSmallerThanPreferredSize(detailed_dialog_widget,
                                                  kDetailedMaxShrinkage)) {
    delegate_weak_ptr->CloseDialogAsIgnored();
    return;
  }
  delegate_weak_ptr->OnWidgetShownStartTracking(detailed_dialog_widget);

  base::RecordAction(base::UserMetricsAction("WebAppDetailedInstallShown"));

#if BUILDFLAG(IS_CHROMEOS)
  webapps::AppId app_id = web_app::GenerateAppIdFromManifestId(manifest_id);
  metrics::structured::StructuredMetricsClient::Record(
      cros_events::AppDiscovery_Browser_AppInstallDialogShown().SetAppId(
          app_id));
#endif  // BUILDFLAG(IS_CHROMEOS)
}

}  // namespace web_app
