// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/installable_payment_app_crawler.h"

#include <limits>
#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/payments/content/icon/icon_size.h"
#include "components/payments/core/features.h"
#include "components/payments/core/native_error_strings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/manifest_icon_downloader.h"
#include "content/public/browser/payment_app_provider_util.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_result.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/blink/public/common/manifest/manifest_icon_selector.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace payments {

RefetchedIcon::RefetchedIcon() = default;
RefetchedIcon::~RefetchedIcon() = default;

// TODO(crbug.com/40548519): Use cache to accelerate crawling procedure.
InstallablePaymentAppCrawler::InstallablePaymentAppCrawler(
    const url::Origin& merchant_origin,
    content::RenderFrameHost* initiator_render_frame_host,
    PaymentManifestDownloader* downloader,
    PaymentManifestParser* parser,
    PaymentManifestWebDataService* cache)
    : log_(content::WebContents::FromRenderFrameHost(
          initiator_render_frame_host)),
      merchant_origin_(merchant_origin),
      initiator_frame_routing_id_(initiator_render_frame_host->GetGlobalId()),
      downloader_(downloader),
      parser_(parser),
      number_of_payment_method_manifest_to_download_(0),
      number_of_payment_method_manifest_to_parse_(0),
      number_of_web_app_manifest_to_download_(0),
      number_of_web_app_manifest_to_parse_(0),
      number_of_web_app_icons_to_download_and_decode_(0) {}

InstallablePaymentAppCrawler::~InstallablePaymentAppCrawler() = default;

void InstallablePaymentAppCrawler::Start(
    const std::vector<mojom::PaymentMethodDataPtr>& requested_method_data,
    std::set<GURL> method_manifest_urls_for_icon_refetch,
    FinishedCrawlingCallback callback,
    base::OnceClosure finished_using_resources) {
  callback_ = std::move(callback);
  finished_using_resources_ = std::move(finished_using_resources);

  std::set<GURL> manifests_to_download;
  if (method_manifest_urls_for_icon_refetch.empty()) {
    // Crawl for JIT installable web apps.
    crawling_mode_ = CrawlingMode::kJustInTimeInstallation;
    for (const auto& method_data : requested_method_data) {
      if (!base::IsStringUTF8(method_data->supported_method))
        continue;
      GURL url = GURL(method_data->supported_method);
      if (url.is_valid()) {
        manifests_to_download.insert(url);
      }
    }
  } else {
    // Crawl to refetch missing icons of already installed apps.
    crawling_mode_ = CrawlingMode::kMissingIconRefetch;
    method_manifest_urls_for_icon_refetch_ =
        std::move(method_manifest_urls_for_icon_refetch);
    for (const auto& method : method_manifest_urls_for_icon_refetch_) {
      DCHECK(method.is_valid());
      manifests_to_download.insert(method);
    }
  }

  if (manifests_to_download.empty()) {
    // Post the result back asynchronously.
    PostTaskToFinishCrawlingPaymentAppsIfReady();
    return;
  }

  // May cause this InstallablePaymentAppCrawler object to be synchronously
  // deleted in the last iteration, so no code should come after the loop.
  number_of_payment_method_manifest_to_download_ = manifests_to_download.size();
  for (const auto& url : manifests_to_download) {
    downloader_->DownloadPaymentMethodManifest(
        merchant_origin_, url,
        base::BindOnce(
            &InstallablePaymentAppCrawler::OnPaymentMethodManifestDownloaded,
            weak_ptr_factory_.GetWeakPtr(), url));
  }
}

void InstallablePaymentAppCrawler::IgnorePortInOriginComparisonForTesting() {
  ignore_port_in_origin_comparison_for_testing_ = true;
}

bool InstallablePaymentAppCrawler::IsSameOriginWith(const GURL& a,
                                                    const GURL& b) {
  if (ignore_port_in_origin_comparison_for_testing_) {
    GURL::Replacements replacements;
    replacements.ClearPort();
    return url::IsSameOriginWith(a.ReplaceComponents(replacements),
                                 b.ReplaceComponents(replacements));
  }
  return url::IsSameOriginWith(a, b);
}

void InstallablePaymentAppCrawler::OnPaymentMethodManifestDownloaded(
    const GURL& method_manifest_url,
    const GURL& method_manifest_url_after_redirects,
    const std::string& content,
    const std::string& error_message) {
  // Enforced in PaymentManifestDownloader.
  DCHECK(net::registry_controlled_domains::SameDomainOrHost(
      method_manifest_url, method_manifest_url_after_redirects,
      net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES));

  number_of_payment_method_manifest_to_download_--;
  if (content.empty()) {
    SetFirstError(error_message);
    FinishCrawlingPaymentAppsIfReady();
    return;
  }

  number_of_payment_method_manifest_to_parse_++;
  parser_->ParsePaymentMethodManifest(
      method_manifest_url, content,
      base::BindOnce(
          &InstallablePaymentAppCrawler::OnPaymentMethodManifestParsed,
          weak_ptr_factory_.GetWeakPtr(), method_manifest_url,
          method_manifest_url_after_redirects, content));
}

void InstallablePaymentAppCrawler::OnPaymentMethodManifestParsed(
    const GURL& method_manifest_url,
    const GURL& method_manifest_url_after_redirects,
    const std::string& content,
    const std::vector<GURL>& default_applications,
    const std::vector<url::Origin>& supported_origins) {
  number_of_payment_method_manifest_to_parse_--;

  auto* rfh = content::RenderFrameHost::FromID(initiator_frame_routing_id_);
  if (!rfh)
    return;

  content::PermissionController* permission_controller =
      rfh->GetBrowserContext()->GetPermissionController();
  DCHECK(permission_controller);

  // If there are no valid entries in default_applications, this task will
  // finish the crawling.
  PostTaskToFinishCrawlingPaymentAppsIfReady();

  // The `DownloadWebAppManifest()` method may synchronously call
  // `OnPaymentWebAppManifestDownloaded()`, e.g., if the owning page has gone
  // away already. This may result in this InstallablePaymentAppCrawler object
  // to be deleted, so no code should be run after this loop.
  //
  // Note that only the last iteration of the loop can result in a deletion, as
  // `number_of_web_app_manifest_to_download_` must be zero for this to happen.
  number_of_web_app_manifest_to_download_ += default_applications.size();
  for (const auto& web_app_manifest_url : default_applications) {
    if (downloaded_web_app_manifests_.find(web_app_manifest_url) !=
        downloaded_web_app_manifests_.end()) {
      // Do not download the same web app manifest again since a web app could
      // be the default application of multiple payment methods.
      number_of_web_app_manifest_to_download_--;
      continue;
    }

    if (!IsSameOriginWith(method_manifest_url_after_redirects,
                          web_app_manifest_url)) {
      number_of_web_app_manifest_to_download_--;
      std::string error_message = base::ReplaceStringPlaceholders(
          errors::kCrossOriginWebAppManifestNotAllowed,
          {web_app_manifest_url.spec(),
           method_manifest_url_after_redirects.spec()},
          nullptr);
      SetFirstError(error_message);
      continue;
    }

    if (permission_controller
            ->GetPermissionResultForOriginWithoutContext(
                blink::PermissionType::PAYMENT_HANDLER,
                url::Origin::Create(web_app_manifest_url))
            .status != blink::mojom::PermissionStatus::GRANTED) {
      // Do not download the web app manifest if it is blocked.
      number_of_web_app_manifest_to_download_--;
      continue;
    }

    downloaded_web_app_manifests_.insert(web_app_manifest_url);

    if (method_manifest_url_after_redirects == web_app_manifest_url) {
      // For this to happen, the payment manifest must have been valid which
      // means its content should have been non-empty. If somehow we get here
      // but content is empty, it would cause a synchronous deletion of 'this',
      // so guard against that.
      CHECK(!content.empty());
      OnPaymentWebAppManifestDownloaded(
          method_manifest_url, web_app_manifest_url, web_app_manifest_url,
          content, /*error_message=*/"");
      continue;
    }

    // May cause this InstallablePaymentAppCrawler object to be synchronously
    // deleted in the last iteration of the loop.
    downloader_->DownloadWebAppManifest(
        url::Origin::Create(method_manifest_url_after_redirects),
        web_app_manifest_url,
        base::BindOnce(
            &InstallablePaymentAppCrawler::OnPaymentWebAppManifestDownloaded,
            weak_ptr_factory_.GetWeakPtr(), method_manifest_url,
            web_app_manifest_url));
  }
}

void InstallablePaymentAppCrawler::OnPaymentWebAppManifestDownloaded(
    const GURL& method_manifest_url,
    const GURL& web_app_manifest_url,
    const GURL& web_app_manifest_url_after_redirects,
    const std::string& content,
    const std::string& error_message) {
#if DCHECK_IS_ON()
  GURL::Replacements replacements;
  if (ignore_port_in_origin_comparison_for_testing_)
    replacements.ClearPort();

  // Enforced in PaymentManifestDownloader.
  DCHECK_EQ(
      web_app_manifest_url.ReplaceComponents(replacements),
      web_app_manifest_url_after_redirects.ReplaceComponents(replacements));
#endif  // DCHECK_IS_ON()

  number_of_web_app_manifest_to_download_--;
  if (content.empty()) {
    SetFirstError(error_message);
    FinishCrawlingPaymentAppsIfReady();
    return;
  }

  number_of_web_app_manifest_to_parse_++;
  parser_->ParseWebAppInstallationInfo(
      content,
      base::BindOnce(
          &InstallablePaymentAppCrawler::OnPaymentWebAppInstallationInfo,
          weak_ptr_factory_.GetWeakPtr(), method_manifest_url,
          web_app_manifest_url));
}

void InstallablePaymentAppCrawler::OnPaymentWebAppInstallationInfo(
    const GURL& method_manifest_url,
    const GURL& web_app_manifest_url,
    std::unique_ptr<WebAppInstallationInfo> app_info,
    std::unique_ptr<std::vector<PaymentManifestParser::WebAppIcon>> icons) {
  number_of_web_app_manifest_to_parse_--;

  // Only download and decode payment app's icon if it is valid and stored.
  if (CompleteAndStorePaymentWebAppInfoIfValid(
          method_manifest_url, web_app_manifest_url, std::move(app_info))) {
    if (!DownloadAndDecodeWebAppIcon(method_manifest_url, web_app_manifest_url,
                                     std::move(icons)) &&
        crawling_mode_ == CrawlingMode::kJustInTimeInstallation &&
        !base::FeatureList::IsEnabled(
            features::kAllowJITInstallationWhenAppIconIsMissing)) {
      std::string error_message = base::ReplaceStringPlaceholders(
          errors::kInvalidWebAppIcon, {web_app_manifest_url.spec()}, nullptr);
      SetFirstError(error_message);
      // App without a valid icon is not JIT installable.
      installable_apps_.erase(method_manifest_url);
    }
  }

  FinishCrawlingPaymentAppsIfReady();
}

bool InstallablePaymentAppCrawler::CompleteAndStorePaymentWebAppInfoIfValid(
    const GURL& method_manifest_url,
    const GURL& web_app_manifest_url,
    std::unique_ptr<WebAppInstallationInfo> app_info) {
  if (app_info == nullptr)
    return false;

  if (app_info->sw_js_url.empty() || !base::IsStringUTF8(app_info->sw_js_url)) {
    SetFirstError(errors::kInvalidServiceWorkerUrl);
    return false;
  }

  // Check and complete relative url.
  if (!GURL(app_info->sw_js_url).is_valid()) {
    GURL absolute_url = web_app_manifest_url.Resolve(app_info->sw_js_url);
    if (!absolute_url.is_valid()) {
      SetFirstError(base::ReplaceStringPlaceholders(
          errors::kCannotResolveServiceWorkerUrl,
          {app_info->sw_js_url, web_app_manifest_url.spec()}, nullptr));
      return false;
    }
    app_info->sw_js_url = absolute_url.spec();
  }

  if (!IsSameOriginWith(web_app_manifest_url, GURL(app_info->sw_js_url))) {
    SetFirstError(base::ReplaceStringPlaceholders(
        errors::kCrossOriginServiceWorkerUrlNotAllowed,
        {app_info->sw_js_url, web_app_manifest_url.spec()}, nullptr));
    return false;
  }

  if (!app_info->sw_scope.empty() && !base::IsStringUTF8(app_info->sw_scope)) {
    SetFirstError(errors::kInvalidServiceWorkerScope);
    return false;
  }

  if (!GURL(app_info->sw_scope).is_valid()) {
    GURL absolute_scope =
        web_app_manifest_url.GetWithoutFilename().Resolve(app_info->sw_scope);
    if (!absolute_scope.is_valid()) {
      SetFirstError(base::ReplaceStringPlaceholders(
          errors::kCannotResolveServiceWorkerScope,
          {app_info->sw_scope, web_app_manifest_url.spec()}, nullptr));
      return false;
    }
    app_info->sw_scope = absolute_scope.spec();
  }

  if (!IsSameOriginWith(web_app_manifest_url, GURL(app_info->sw_scope))) {
    SetFirstError(base::ReplaceStringPlaceholders(
        errors::kCrossOriginServiceWorkerScopeNotAllowed,
        {app_info->sw_scope, web_app_manifest_url.spec()}, nullptr));
    return false;
  }

  std::string error_message;
  if (!content::PaymentAppProviderUtil::IsValidInstallablePaymentApp(
          web_app_manifest_url, GURL(app_info->sw_js_url),
          GURL(app_info->sw_scope), &error_message)) {
    SetFirstError(error_message);
    return false;
  }

  // TODO(crbug.com/40548519): Support multiple installable payment apps for a
  // payment method.
  if (installable_apps_.find(method_manifest_url) != installable_apps_.end()) {
    SetFirstError(errors::kInstallingMultipleDefaultAppsNotSupported);
    return false;
  }

  // If this is called by tests, convert service worker URLs back to test server
  // URLs so the service worker code can be fetched and installed. This is done
  // last so same-origin checks are performed on the "logical" URLs instead of
  // real test URLs with ports.
  if (ignore_port_in_origin_comparison_for_testing_) {
    app_info->sw_js_url =
        downloader_->FindTestServerURL(GURL(app_info->sw_js_url)).spec();
    app_info->sw_scope =
        downloader_->FindTestServerURL(GURL(app_info->sw_scope)).spec();
  }

  if (crawling_mode_ == CrawlingMode::kJustInTimeInstallation) {
    installable_apps_[method_manifest_url] = std::move(app_info);
  }

  return true;
}

bool InstallablePaymentAppCrawler::DownloadAndDecodeWebAppIcon(
    const GURL& method_manifest_url,
    const GURL& web_app_manifest_url,
    std::unique_ptr<std::vector<PaymentManifestParser::WebAppIcon>> icons) {
  if (icons == nullptr || icons->empty()) {
    log_.Warn(
        "No valid icon information for installable payment handler found in "
        "web app manifest \"" +
        web_app_manifest_url.spec() + "\" for payment handler manifest \"" +
        method_manifest_url.spec() + "\".");
    return false;
  }

  std::vector<blink::Manifest::ImageResource> manifest_icons;
  for (const auto& icon : *icons) {
    if (icon.src.empty() || !base::IsStringUTF8(icon.src)) {
      log_.Warn(
          "The installable payment handler's icon src URL is not a non-empty "
          "UTF8 string in web app manifest \"" +
          web_app_manifest_url.spec() + "\" for payment handler manifest \"" +
          method_manifest_url.spec() + "\".");
      continue;
    }

    GURL icon_src = GURL(icon.src);
    if (!icon_src.is_valid()) {
      icon_src = web_app_manifest_url.Resolve(icon.src);
      if (!icon_src.is_valid()) {
        log_.Warn(
            "Failed to resolve the installable payment handler's icon src url "
            "\"" +
            icon.src + "\" in web app manifest \"" +
            web_app_manifest_url.spec() + "\" for payment handler manifest \"" +
            method_manifest_url.spec() + "\".");
        continue;
      }
    }

    blink::Manifest::ImageResource manifest_icon;
    manifest_icon.src = icon_src;
    manifest_icon.type = base::UTF8ToUTF16(icon.type);
    manifest_icon.purpose.emplace_back(
        blink::mojom::ManifestImageResource_Purpose::ANY);
    // TODO(crbug.com/40548519): Parse icon sizes.
    manifest_icon.sizes.emplace_back(gfx::Size());
    manifest_icons.emplace_back(manifest_icon);
  }

  if (manifest_icons.empty()) {
    log_.Warn("No valid icons found in web app manifest \"" +
              web_app_manifest_url.spec() +
              "\" for payment handler manifest \"" +
              method_manifest_url.spec() + "\".");
    return false;
  }

  // If the initiator frame doesn't exists any more, e.g. the frame has
  // navigated away, don't download the icon.
  // TODO(crbug.com/40121328): Move this sanity check to ManifestIconDownloader
  // after DownloadImage refactor is done.
  auto* rfh = content::RenderFrameHost::FromID(initiator_frame_routing_id_);
  auto* web_contents = rfh && rfh->IsActive()
                           ? content::WebContents::FromRenderFrameHost(rfh)
                           : nullptr;
  if (!web_contents) {
    log_.Warn(
        "Cannot download icons after the webpage has been closed (web app "
        "manifest \"" +
        web_app_manifest_url.spec() + "\" for payment handler manifest \"" +
        method_manifest_url.spec() + "\").");
    // Post the result back asynchronously.
    PostTaskToFinishCrawlingPaymentAppsIfReady();
    return false;
  }

  gfx::NativeView native_view = web_contents->GetNativeView();
  GURL best_icon_url = blink::ManifestIconSelector::FindBestMatchingIcon(
      manifest_icons, IconSizeCalculator::IdealIconHeight(native_view),
      IconSizeCalculator::MinimumIconHeight(),
      content::ManifestIconDownloader::kMaxWidthToHeightRatio,
      blink::mojom::ManifestImageResource_Purpose::ANY);
  if (!best_icon_url.is_valid()) {
    log_.Warn("No suitable icon found in web app manifest \"" +
              web_app_manifest_url.spec() +
              "\" for payment handler manifest \"" +
              method_manifest_url.spec() + "\".");
    return false;
  }

  number_of_web_app_icons_to_download_and_decode_++;

  bool can_download_icon = content::ManifestIconDownloader::Download(
      web_contents, downloader_->FindTestServerURL(best_icon_url),
      IconSizeCalculator::IdealIconHeight(native_view),
      IconSizeCalculator::MinimumIconHeight(),
      /* maximum_icon_size_in_px= */ std::numeric_limits<int>::max(),
      base::BindOnce(
          &InstallablePaymentAppCrawler::OnPaymentWebAppIconDownloadAndDecoded,
          weak_ptr_factory_.GetWeakPtr(), method_manifest_url,
          web_app_manifest_url),
      false, /* square_only */
      initiator_frame_routing_id_);
  DCHECK(can_download_icon);
  return can_download_icon;
}

void InstallablePaymentAppCrawler::OnPaymentWebAppIconDownloadAndDecoded(
    const GURL& method_manifest_url,
    const GURL& web_app_manifest_url,
    const SkBitmap& icon) {
  number_of_web_app_icons_to_download_and_decode_--;

  if (crawling_mode_ == CrawlingMode::kJustInTimeInstallation) {
    auto it = installable_apps_.find(method_manifest_url);
    CHECK(it != installable_apps_.end(), base::NotFatalUntil::M130);
    DCHECK(IsSameOriginWith(GURL(it->second->sw_scope), web_app_manifest_url));
    if (icon.drawsNothing() &&
        !base::FeatureList::IsEnabled(
            features::kAllowJITInstallationWhenAppIconIsMissing)) {
      log_.Error(
          "Failed to download or decode the icon from web app manifest \"" +
          web_app_manifest_url.spec() + "\" for payment handler manifest \"" +
          method_manifest_url.spec() + "\".");
      std::string error_message = base::ReplaceStringPlaceholders(
          errors::kInvalidWebAppIcon, {web_app_manifest_url.spec()}, nullptr);
      SetFirstError(error_message);
      installable_apps_.erase(it);
    } else {
      it->second->icon = std::make_unique<SkBitmap>(icon);
    }
  } else {
    DCHECK_EQ(CrawlingMode::kMissingIconRefetch, crawling_mode_);
    auto it = method_manifest_urls_for_icon_refetch_.find(method_manifest_url);
    CHECK(it != method_manifest_urls_for_icon_refetch_.end(),
          base::NotFatalUntil::M130);
    if (icon.drawsNothing()) {
      log_.Warn("Failed to refetch a valid icon from web app manifest \"" +
                web_app_manifest_url.spec() +
                "\" for payment handler manifest \"" +
                method_manifest_url.spec() + "\".");
    } else {
      auto refetched_icon = std::make_unique<RefetchedIcon>();
      refetched_icon->method_name = method_manifest_url.spec();
      refetched_icon->icon = std::make_unique<SkBitmap>(icon);
      refetched_icons_.insert(
          std::make_pair(web_app_manifest_url, std::move(refetched_icon)));
    }
  }

  FinishCrawlingPaymentAppsIfReady();
}

void InstallablePaymentAppCrawler::
    PostTaskToFinishCrawlingPaymentAppsIfReady() {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &InstallablePaymentAppCrawler::FinishCrawlingPaymentAppsIfReady,
          weak_ptr_factory_.GetWeakPtr()));
}

void InstallablePaymentAppCrawler::FinishCrawlingPaymentAppsIfReady() {
  if (number_of_payment_method_manifest_to_download_ != 0 ||
      number_of_payment_method_manifest_to_parse_ != 0 ||
      number_of_web_app_manifest_to_download_ != 0 ||
      number_of_web_app_manifest_to_parse_ != 0 ||
      number_of_web_app_icons_to_download_and_decode_ != 0) {
    return;
  }

  std::move(callback_).Run(std::move(installable_apps_),
                           std::move(refetched_icons_), first_error_message_);
  std::move(finished_using_resources_).Run();
}

void InstallablePaymentAppCrawler::SetFirstError(
    const std::string& error_message) {
  log_.Error(error_message);
  if (first_error_message_.empty())
    first_error_message_ = error_message;
}

}  // namespace payments.
