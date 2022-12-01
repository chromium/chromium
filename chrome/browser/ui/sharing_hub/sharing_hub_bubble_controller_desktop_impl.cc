// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sharing_hub/sharing_hub_bubble_controller_desktop_impl.h"

#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/image_fetcher/image_decoder_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/share/share_features.h"
#include "chrome/browser/share/share_metrics.h"
#include "chrome/browser/sharing_hub/sharing_hub_features.h"
#include "chrome/browser/sharing_hub/sharing_hub_model.h"
#include "chrome/browser/sharing_hub/sharing_hub_service.h"
#include "chrome/browser/sharing_hub/sharing_hub_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/qrcode_generator/qrcode_generator_bubble_controller.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_bubble.h"
#include "chrome/browser/ui/sharing_hub/sharing_hub_bubble_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/image_fetcher/core/image_fetcher_impl.h"
#include "components/media_router/browser/media_router_dialog_controller.h"
#include "components/media_router/browser/media_router_metrics.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/opengraph/metadata.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_provider.h"
#include "ui/native_theme/native_theme.h"

namespace sharing_hub {

namespace {

constexpr char kPreviewUmaClient[] = "SharePreview";

constexpr net::NetworkTrafficAnnotationTag kPreviewImageNetworkAnnotationTag =
    net::DefineNetworkTrafficAnnotation("share_preview_image_fetch",
                                        R"(
      semantics {
        sender: "Share bubble"
        description:
            "The share bubble offers a preview of the site or image being "
            "shared. For sites, this image is specified by the site author "
            "using OpenGraph metadata. If this metadata is present on the "
            "site being shared and specifies a preview image, the share "
            "bubble fetches the image to display it."
        trigger:
            "User presses 'Share' on a page that has OpenGraph metadata."
        data:
            "The image URL being requested from the site. Since the user has "
            "already visited the site to trigger the sharing flow, this "
            "request is similar to a request for any other part of the page."
        destination: WEBSITE
      }
      policy {
        cookies_allowed: NO
        setting:
          "Administrators can disable this feature by disabling the share "
          "bubble altogether, which can be done via policy. There is no "
          "specific way to disable loading the preview image."
        chrome_policy: {
          DesktopSharingHubEnabled: {
            DesktopSharingHubEnabled: false
          }
        }
      }
  )");

bool ShouldUseHQPreviewImage() {
  auto variant = share::GetDesktopSharePreviewVariant();
  return variant != share::DesktopSharePreviewVariant::kDisabled &&
         variant != share::DesktopSharePreviewVariant::kEnabled16;
}

}  // namespace

// static
// SharingHubBubbleController:
SharingHubBubbleController*
SharingHubBubbleController::CreateOrGetFromWebContents(
    content::WebContents* web_contents) {
  SharingHubBubbleControllerDesktopImpl::CreateForWebContents(web_contents);
  SharingHubBubbleControllerDesktopImpl* controller =
      SharingHubBubbleControllerDesktopImpl::FromWebContents(web_contents);
  return controller;
}

SharingHubBubbleControllerDesktopImpl::
    ~SharingHubBubbleControllerDesktopImpl() {
  if (sharing_hub_bubble_view_) {
    sharing_hub_bubble_view_->Hide();
  }
}

void SharingHubBubbleControllerDesktopImpl::HideBubble() {
  if (sharing_hub_bubble_view_) {
    sharing_hub_bubble_view_->Hide();
    sharing_hub_bubble_view_ = nullptr;
  }
}

void SharingHubBubbleControllerDesktopImpl::ShowBubble(
    share::ShareAttempt attempt) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());

  sharing_hub_bubble_view_ = browser->window()->ShowSharingHubBubble(attempt);

  if (ShouldUsePreview())
    FetchImageForPreview();

  share::LogShareSourceDesktop(share::ShareSourceDesktop::kOmniboxSharingHub);
}

SharingHubBubbleView*
SharingHubBubbleControllerDesktopImpl::sharing_hub_bubble_view() const {
  return sharing_hub_bubble_view_;
}

bool SharingHubBubbleControllerDesktopImpl::ShouldOfferOmniboxIcon() {
  return SharingHubOmniboxEnabled(GetWebContents().GetBrowserContext());
}

std::u16string SharingHubBubbleControllerDesktopImpl::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_SHARING_HUB_TITLE);
}

Profile* SharingHubBubbleControllerDesktopImpl::GetProfile() const {
  return Profile::FromBrowserContext(web_contents()->GetBrowserContext());
}

std::vector<SharingHubAction>
SharingHubBubbleControllerDesktopImpl::GetFirstPartyActions() {
  std::vector<SharingHubAction> actions;

  SharingHubModel* model = GetSharingHubModel();
  if (model)
    model->GetFirstPartyActionList(&GetWebContents(), &actions);

  return actions;
}

std::vector<SharingHubAction>
SharingHubBubbleControllerDesktopImpl::GetThirdPartyActions() {
  std::vector<SharingHubAction> actions;

  SharingHubModel* model = GetSharingHubModel();
  if (model)
    model->GetThirdPartyActionList(&GetWebContents(), &actions);

  return actions;
}

bool SharingHubBubbleControllerDesktopImpl::ShouldUsePreview() {
  return share::GetDesktopSharePreviewVariant() !=
         share::DesktopSharePreviewVariant::kDisabled;
}

base::CallbackListSubscription
SharingHubBubbleControllerDesktopImpl::RegisterPreviewImageChangedCallback(
    PreviewImageChangedCallback callback) {
  return preview_image_changed_callbacks_.Add(callback);
}

base::WeakPtr<SharingHubBubbleController>
SharingHubBubbleControllerDesktopImpl::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void SharingHubBubbleControllerDesktopImpl::OnActionSelected(
    int command_id,
    bool is_first_party,
    std::string feature_name_for_metrics) {
  Browser* browser = chrome::FindBrowserWithWebContents(&GetWebContents());
  // Can be null in tests.
  if (!browser)
    return;

  if (is_first_party) {
    base::RecordComputedAction(feature_name_for_metrics);

    // Show a back button for 1P dialogs anchored to the sharing hub icon.
    if (command_id == IDC_QRCODE_GENERATOR) {
      qrcode_generator::QRCodeGeneratorBubbleController* qrcode_controller =
          qrcode_generator::QRCodeGeneratorBubbleController::Get(
              &GetWebContents());
      qrcode_controller->ShowBubble(GetWebContents().GetLastCommittedURL(),
                                    true);
    } else if (command_id == IDC_SEND_TAB_TO_SELF) {
      send_tab_to_self::ShowBubble(&GetWebContents(),
                                   /*show_back_button=*/true);
    } else if (command_id == IDC_ROUTE_MEDIA) {
      media_router::MediaRouterDialogController* dialog_controller =
          media_router::MediaRouterDialogController::GetOrCreateForWebContents(
              browser->tab_strip_model()->GetActiveWebContents());
      if (!dialog_controller)
        return;

      dialog_controller->ShowMediaRouterDialog(
          media_router::MediaRouterDialogActivationLocation::SHARING_HUB);
    } else {
      chrome::ExecuteCommand(browser, command_id);
    }
  } else {
    SharingHubModel* model = GetSharingHubModel();
    DCHECK(model);
    model->ExecuteThirdPartyAction(&GetWebContents(), command_id);
  }
}

void SharingHubBubbleControllerDesktopImpl::OnBubbleClosed() {
  sharing_hub_bubble_view_ = nullptr;
}

SharingHubModel* SharingHubBubbleControllerDesktopImpl::GetSharingHubModel() {
  if (!sharing_hub_model_) {
    SharingHubService* const service =
        SharingHubServiceFactory::GetForProfile(GetProfile());
    if (!service)
      return nullptr;
    sharing_hub_model_ = service->GetSharingHubModel();
  }
  return sharing_hub_model_;
}

void SharingHubBubbleControllerDesktopImpl::FetchImageForPreview() {
  if (ShouldUseHQPreviewImage())
    FetchHQImageForPreview();
  else
    FetchFaviconForPreview();
}

void SharingHubBubbleControllerDesktopImpl::FetchFaviconForPreview() {
  content::WebContents* web_contents = &GetWebContents();
  gfx::Image favicon = favicon::TabFaviconFromWebContents(web_contents);
  if (favicon.IsEmpty())
    return;

  content::NavigationController& controller = web_contents->GetController();
  content::NavigationEntry* entry = controller.GetLastCommittedEntry();
  // Select chrome URLs show themified icons in dark mode.
  if (favicon::ShouldThemifyFaviconForEntry(entry) &&
      ui::NativeTheme::GetInstanceForNativeUi()->ShouldUseDarkColors()) {
    const ui::ColorProvider& color_provider = web_contents->GetColorProvider();
    favicon = gfx::Image(favicon::ThemeFavicon(
        *favicon.ToImageSkia(),
        color_provider.GetColor(kColorToolbarButtonIcon),
        color_provider.GetColor(kColorTabBackgroundActiveFrameActive),
        color_provider.GetColor(kColorTabBackgroundInactiveFrameActive)));
  }

  preview_image_changed_callbacks_.Notify(ui::ImageModel::FromImage(favicon));
}

void SharingHubBubbleControllerDesktopImpl::FetchHQImageForPreview() {
  content::RenderFrameHost& main_frame =
      GetWebContents().GetPrimaryPage().GetMainDocument();
  main_frame.GetOpenGraphMetadata(base::BindOnce(
      &SharingHubBubbleControllerDesktopImpl::OnGetOpenGraphMetadata,
      internal_weak_factory_.GetWeakPtr()));
}

void SharingHubBubbleControllerDesktopImpl::OnGetOpenGraphMetadata(
    blink::mojom::OpenGraphMetadataPtr metadata) {
  if (!metadata->image) {
    FetchFaviconForPreview();
    return;
  }

  auto* profile =
      Profile::FromBrowserContext(GetWebContents().GetBrowserContext());
  if (!profile) {
    // No fallback to the favicon for this case: if the profile's gone, the
    // favicon service will be too. Just use the default page icon for the
    // preview.
    return;
  }

  image_fetcher_ = std::make_unique<image_fetcher::ImageFetcherImpl>(
      std::make_unique<ImageDecoderImpl>(),
      profile->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess());

  image_fetcher_->FetchImage(
      *metadata->image,
      base::BindOnce(&SharingHubBubbleControllerDesktopImpl::OnGetHQImage,
                     internal_weak_factory_.GetWeakPtr()),
      image_fetcher::ImageFetcherParams(kPreviewImageNetworkAnnotationTag,
                                        kPreviewUmaClient));
}

void SharingHubBubbleControllerDesktopImpl::OnGetHQImage(
    const gfx::Image& image,
    const image_fetcher::RequestMetadata& metadata) {
  if (!image.IsEmpty())
    preview_image_changed_callbacks_.Notify(ui::ImageModel::FromImage(image));
  else
    FetchFaviconForPreview();
}

SharingHubBubbleControllerDesktopImpl::SharingHubBubbleControllerDesktopImpl(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<SharingHubBubbleControllerDesktopImpl>(
          *web_contents) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SharingHubBubbleControllerDesktopImpl);

}  // namespace sharing_hub
