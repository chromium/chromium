// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/installable_payment_app_crawler.h"

#include <utility>

#include "base/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "components/payments/content/icon/icon_size.h"
#include "components/payments/core/native_error_strings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/manifest_icon_downloader.h"
#include "content/public/browser/payment_app_provider.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/web_contents.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/common/manifest/manifest_icon_selector.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace payments {

// TODO(crbug.com/782270): Use cache to accelerate crawling procedure.
// TODO(crbug.com/782270): Add integration tests for this class.
InstallablePaymentAppCrawler::InstallablePaymentAppCrawler(
    content::WebContents* web_contents,
    PaymentManifestDownloader* downloader,
    PaymentManifestParser* parser,
    PaymentManifestWebDataService* cache)
    : WebContentsObserver(web_contents),
      log_(web_contents),
      downloader_(downloader),
      parser_(parser),
      number_of_payment_method_manifest_to_download_(0),
      number_of_payment_method_manifest_to_parse_(0),
      number_of_web_app_manifest_to_download_(0),
      number_of_web_app_manifest_to_parse_(0),
      number_of_web_app_icons_to_download_and_decode_(0) {}

InstallablePaymentAppCrawler::~InstallablePaymentAppCrawler() {}

void InstallablePaymentAppCrawler::Start(
    const std::vector<mojom::PaymentMethodDataPtr>& requested_method_data,
    FinishedCrawlingCallback callback,
    base::OnceClosure finished_using_resources) {
  callback_ = std::move(callback);
  finished_using_resources_ = std::move(finished_using_resources);

  std::set<GURL> manifests_to_download;
  for (const auto& method_data : requested_method_data) {
    if (!base::IsStringUTF8(method_data->supported_method))
      continue;
    GURL url = GURL(method_data->supported_method);
    if (url.is_valid()) {
      manifests_to_download.insert(url);
    }
  }

  if (manifests_to_download.empty()) {
    // Post the result back asynchronously.
    base::PostTask(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(
            &InstallablePaymentAppCrawler::FinishCrawlingPaymentAppsIfReady,
            weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  number_of_payment_method_manifest_to_download_ = manifests_to_download.size();
  for (const auto& url : manifests_to_download) {
    downloader_->DownloadPaymentMethodManifest(
        url,
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
      content, base::BindOnce(
                   &InstallablePaymentAppCrawler::OnPaymentMethodManifestParsed,
                   weak_ptr_factory_.GetWeakPtr(), method_manifest_url,
                   method_manifest_url_after_redirects));
}

void InstallablePaymentAppCrawler::OnPaymentMethodManifestParsed(
    const GURL& method_manifest_url,
    const GURL& method_manifest_url_after_redirects,
    const std::vector<GURL>& default_applications,
    const std::vector<url::Origin>& supported_origins,
    bool all_origins_supported) {
  number_of_payment_method_manifest_to_parse_--;

  if (web_contents() == nullptr)
    return;
  content::PermissionController* permission_controller =
      content::BrowserContext::GetPermissionController(
          web_contents()->GetBrowserContext());
  DCHECK(permission_controller);

  for (const auto& web_app_manifest_url : default_applications) {
    if (downloaded_web_app_manifests_.find(web_app_manifest_url) !=
        downloaded_web_app_manifests_.end()) {
      // Do not download the same web app manifest again since a web app could
      // be the default application of multiple payment methods.
      continue;
    }

    if (!IsSameOriginWith(method_manifest_url_after_redirects,
                          web_app_manifest_url)) {
      std::string error_message = base::ReplaceStringPlaceholders(
          errors::kCrossOriginWebAppManifestNotAllowed,
          {web_app_manifest_url.spec(),
           method_manifest_url_after_redirects.spec()},
          nullptr);
      SetFirstError(error_message);
      continue;
    }

    if (permission_controller->GetPermissionStatus(
            content::PermissionType::PAYMENT_HANDLER,
            web_app_manifest_url.GetOrigin(),
            web_app_manifest_url.GetOrigin()) !=
        blink::mojom::PermissionStatus::GRANTED) {
      // Do not download the web app manifest if it is blocked.
      continue;
    }

    number_of_web_app_manifest_to_download_++;
    downloaded_web_app_manifests_.insert(web_app_manifest_url);
    downloader_->DownloadWebAppManifest(
        web_app_manifest_url,
        base::BindOnce(
            &InstallablePaymentAppCrawler::OnPaymentWebAppManifestDownloaded,
            weak_ptr_factory_.GetWeakPtr(), method_manifest_url,
            web_app_manifest_url));
  }

  FinishCrawlingPaymentAppsIfReady();
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

  if (CompleteAndStorePaymentWebAppInfoIfValid(
          method_manifest_url, web_app_manifest_url, std::move(app_info))) {
    // Only download and decode payment app's icon if it is valid and stored.
    DownloadAndDecodeWebAppIcon(method_manifest_url, web_app_manifest_url,
                                std::move(icons));
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
  if (!content::PaymentAppProvider::GetInstance()->IsValidInstallablePaymentApp(
          web_app_manifest_url, GURL(app_info->sw_js_url),
          GURL(app_info->sw_scope), &error_message)) {
    SetFirstError(error_message);
    return false;
  }

  // TODO(crbug.com/782270): Support multiple installable payment apps for a
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

  installable_apps_[method_manifest_url] = std::move(app_info);

  return true;
}

void InstallablePaymentAppCrawler::DownloadAndDecodeWebAppIcon(
    const GURL& method_manifest_url,
    const GURL& web_app_manifest_url,
    std::unique_ptr<std::vector<PaymentManifestParser::WebAppIcon>> icons) {
  if (icons == nullptr || icons->empty()) {
    log_.Warn(
        "No valid icon information for installable payment handler found in "
        "web app manifest \"" +
        web_app_manifest_url.spec() + "\" for payment handler manifest \"" +
        method_manifest_url.spec() + "\".");
    return;
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
        blink::Manifest::ImageResource::Purpose::ANY);
    // TODO(crbug.com/782270): Parse icon sizes.
    manifest_icon.sizes.emplace_back(gfx::Size());
    manifest_icons.emplace_back(manifest_icon);
  }

  if (manifest_icons.empty()) {
    log_.Warn("No valid icons found in web app manifest \"" +
              web_app_manifest_url.spec() +
              "\" for payment handler manifest \"" +
              method_manifest_url.spec() + "\".");
    return;
  }

  // Stop if the web_contents is gone.
  if (web_contents() == nullptr) {
    log_.Warn(
        "Cannot download icons after the webpage has been closed (web app "
        "manifest \"" +
        web_app_manifest_url.spec() + "\" for payment handler manifest \"" +
        method_manifest_url.spec() + "\").");
    return;
  }

  gfx::NativeView native_view = web_contents()->GetNativeView();
  GURL best_icon_url = blink::ManifestIconSelector::FindBestMatchingIcon(
      manifest_icons, IconSizeCalculator::IdealIconHeight(native_view),
      IconSizeCalculator::MinimumIconHeight(),
      content::ManifestIconDownloader::kMaxWidthToHeightRatio,
      blink::Manifest::ImageResource::Purpose::ANY);
  if (!best_icon_url.is_valid()) {
    log_.Warn("No suitable icon found in web app manifest \"" +
              web_app_manifest_url.spec() +
              "\" for payment handler manifest \"" +
              method_manifest_url.spec() + "\".");
    return;
  }

  number_of_web_app_icons_to_download_and_decode_++;
  bool can_download_icon = content::ManifestIconDownloader::Download(
      web_contents(), downloader_->FindTestServerURL(best_icon_url),
      IconSizeCalculator::IdealIconHeight(native_view),
      IconSizeCalculator::MinimumIconHeight(),
      base::BindOnce(
          &InstallablePaymentAppCrawler::OnPaymentWebAppIconDownloadAndDecoded,
          weak_ptr_factory_.GetWeakPtr(), method_manifest_url,
          web_app_manifest_url),
      false /* square_only */);
  DCHECK(can_download_icon);
}

void InstallablePaymentAppCrawler::OnPaymentWebAppIconDownloadAndDecoded(
    const GURL& method_manifest_url,
    const GURL& web_app_manifest_url,
    const SkBitmap& icon) {
  number_of_web_app_icons_to_download_and_decode_--;
  if (icon.drawsNothing()) {
    log_.Error(
        "Failed to download or decode the icon from web app manifest \"" +
        web_app_manifest_url.spec() + "\" for payment handler manifest \"" +
        method_manifest_url.spec() + "\".");
  } else {
    auto it = installable_apps_.find(method_manifest_url);
    DCHECK(it != installable_apps_.end());
    DCHECK(IsSameOriginWith(GURL(it->second->sw_scope), web_app_manifest_url));

    it->second->icon = std::make_unique<SkBitmap>(icon);
  }

  FinishCrawlingPaymentAppsIfReady();
}

void InstallablePaymentAppCrawler::FinishCrawlingPaymentAppsIfReady() {
  if (number_of_payment_method_manifest_to_download_ != 0 ||
      number_of_payment_method_manifest_to_parse_ != 0 ||
      number_of_web_app_manifest_to_download_ != 0 ||
      number_of_web_app_manifest_to_parse_ != 0 ||
      number_of_web_app_icons_to_download_and_decode_ != 0) {
    return;
  }

  std::move(callback_).Run(std::move(installable_apps_), first_error_message_);
  std::move(finished_using_resources_).Run();
}

void InstallablePaymentAppCrawler::SetFirstError(
    const std::string& error_message) {
  log_.Error(error_message);
  if (first_error_message_.empty())
    first_error_message_ = error_message;
}

}  // namespace payments.
