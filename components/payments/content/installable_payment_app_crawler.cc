// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/installable_payment_app_crawler.h"

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/manifest_icon_downloader.h"
#include "content/public/browser/payment_app_provider.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/console_message_level.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/common/manifest/manifest_icon_selector.h"
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
      downloader_(downloader),
      parser_(parser),
      number_of_payment_method_manifest_to_download_(0),
      number_of_payment_method_manifest_to_parse_(0),
      number_of_web_app_manifest_to_download_(0),
      number_of_web_app_manifest_to_parse_(0),
      number_of_web_app_icons_to_download_and_decode_(0),
      weak_ptr_factory_(this) {}

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
    base::PostTaskWithTraits(
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

void InstallablePaymentAppCrawler::OnPaymentMethodManifestDownloaded(
    const GURL& method_manifest_url,
    const std::string& content) {
  number_of_payment_method_manifest_to_download_--;
  if (content.empty()) {
    FinishCrawlingPaymentAppsIfReady();
    return;
  }

  number_of_payment_method_manifest_to_parse_++;
  parser_->ParsePaymentMethodManifest(
      content, base::BindOnce(
                   &InstallablePaymentAppCrawler::OnPaymentMethodManifestParsed,
                   weak_ptr_factory_.GetWeakPtr(), method_manifest_url));
}

void InstallablePaymentAppCrawler::OnPaymentMethodManifestParsed(
    const GURL& method_manifest_url,
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

  for (const auto& url : default_applications) {
    if (downloaded_web_app_manifests_.find(url) !=
        downloaded_web_app_manifests_.end()) {
      // Do not download the same web app manifest again since a web app could
      // be the default application of multiple payment methods.
      continue;
    }

    if (!net::registry_controlled_domains::SameDomainOrHost(
            method_manifest_url, url,
            net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)) {
      WarnIfPossible("Installable payment app from " + url.spec() +
                     " is not allowed for the method " +
                     method_manifest_url.spec());
      continue;
    }

    if (permission_controller->GetPermissionStatus(
            content::PermissionType::PAYMENT_HANDLER, url.GetOrigin(),
            url.GetOrigin()) != blink::mojom::PermissionStatus::GRANTED) {
      // Do not download the web app manifest if it is blocked.
      continue;
    }

    number_of_web_app_manifest_to_download_++;
    downloaded_web_app_manifests_.insert(url);
    downloader_->DownloadWebAppManifest(
        url,
        base::BindOnce(
            &InstallablePaymentAppCrawler::OnPaymentWebAppManifestDownloaded,
            weak_ptr_factory_.GetWeakPtr(), method_manifest_url, url));
  }

  FinishCrawlingPaymentAppsIfReady();
}

void InstallablePaymentAppCrawler::OnPaymentWebAppManifestDownloaded(
    const GURL& method_manifest_url,
    const GURL& web_app_manifest_url,
    const std::string& content) {
  number_of_web_app_manifest_to_download_--;
  if (content.empty()) {
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
    WarnIfPossible(
        "The installable payment app's js url is not a non-empty UTF8 string.");
    return false;
  }

  // Check and complete relative url.
  if (!GURL(app_info->sw_js_url).is_valid()) {
    GURL absolute_url = web_app_manifest_url.Resolve(app_info->sw_js_url);
    if (!absolute_url.is_valid()) {
      WarnIfPossible(
          "Failed to resolve the installable payment app's js url (" +
          app_info->sw_js_url + ").");
      return false;
    }
    if (!net::registry_controlled_domains::SameDomainOrHost(
            method_manifest_url, absolute_url,
            net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)) {
      WarnIfPossible("Installable payment app's js url " + absolute_url.spec() +
                     " is not allowed for the method " +
                     method_manifest_url.spec());
      return false;
    }
    app_info->sw_js_url = absolute_url.spec();
  }

  if (!GURL(app_info->sw_scope).is_valid()) {
    GURL absolute_scope =
        web_app_manifest_url.GetWithoutFilename().Resolve(app_info->sw_scope);
    if (!absolute_scope.is_valid()) {
      WarnIfPossible(
          "Failed to resolve the installable payment app's registration "
          "scope (" +
          app_info->sw_scope + ").");
      return false;
    }
    if (!net::registry_controlled_domains::SameDomainOrHost(
            method_manifest_url, absolute_scope,
            net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)) {
      WarnIfPossible("Installable payment app's registration scope " +
                     absolute_scope.spec() + " is not allowed for the method " +
                     method_manifest_url.spec());
      return false;
    }
    app_info->sw_scope = absolute_scope.spec();
  }

  std::string error_message;
  if (!content::PaymentAppProvider::GetInstance()->IsValidInstallablePaymentApp(
          web_app_manifest_url, GURL(app_info->sw_js_url),
          GURL(app_info->sw_scope), &error_message)) {
    WarnIfPossible(error_message);
    return false;
  }

  // TODO(crbug.com/782270): Support multiple installable payment apps for a
  // payment method.
  if (installable_apps_.find(method_manifest_url) != installable_apps_.end())
    return false;

  installable_apps_[method_manifest_url] = std::move(app_info);

  return true;
}

void InstallablePaymentAppCrawler::DownloadAndDecodeWebAppIcon(
    const GURL& method_manifest_url,
    const GURL& web_app_manifest_url,
    std::unique_ptr<std::vector<PaymentManifestParser::WebAppIcon>> icons) {
  if (icons == nullptr || icons->empty())
    return;

  std::vector<blink::Manifest::ImageResource> manifest_icons;
  for (const auto& icon : *icons) {
    if (icon.src.empty() || !base::IsStringUTF8(icon.src)) {
      WarnIfPossible(
          "The installable payment app's icon src url is not a non-empty UTF8 "
          "string.");
      continue;
    }

    GURL icon_src = GURL(icon.src);
    if (!icon_src.is_valid()) {
      icon_src = web_app_manifest_url.Resolve(icon.src);
      if (!icon_src.is_valid()) {
        WarnIfPossible(
            "Failed to resolve the installable payment app's icon src url (" +
            icon.src + ").");
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

  if (manifest_icons.empty())
    return;

  // TODO(crbug.com/782270): Choose appropriate icon size dynamically on
  // different platforms. Here we choose a large ideal icon size to be big
  // enough for all platforms. Note that we only scale down for this icon size
  // but not scale up.
  const int kPaymentAppIdealIconSize = 0xFFFF;
  const int kPaymentAppMinimumIconSize = 0;
  GURL best_icon_url = blink::ManifestIconSelector::FindBestMatchingIcon(
      manifest_icons, kPaymentAppIdealIconSize, kPaymentAppMinimumIconSize,
      blink::Manifest::ImageResource::Purpose::ANY);
  if (!best_icon_url.is_valid()) {
    WarnIfPossible(
        "No suitable icon found in the installabble payment app's manifest (" +
        web_app_manifest_url.spec() + ").");
    return;
  }

  // Stop if the web_contents is gone.
  if (web_contents() == nullptr)
    return;

  number_of_web_app_icons_to_download_and_decode_++;
  bool can_download_icon = content::ManifestIconDownloader::Download(
      web_contents(), best_icon_url, kPaymentAppIdealIconSize,
      kPaymentAppMinimumIconSize,
      base::Bind(
          &InstallablePaymentAppCrawler::OnPaymentWebAppIconDownloadAndDecoded,
          weak_ptr_factory_.GetWeakPtr(), method_manifest_url,
          web_app_manifest_url));
  DCHECK(can_download_icon);
}

void InstallablePaymentAppCrawler::OnPaymentWebAppIconDownloadAndDecoded(
    const GURL& method_manifest_url,
    const GURL& web_app_manifest_url,
    const SkBitmap& icon) {
  number_of_web_app_icons_to_download_and_decode_--;
  if (icon.drawsNothing()) {
    WarnIfPossible(
        "Failed to download or decode installable payment app's icon for web "
        "app manifest " +
        web_app_manifest_url.spec() + ".");
  } else {
    auto it = installable_apps_.find(method_manifest_url);
    DCHECK(it != installable_apps_.end());
    DCHECK(url::IsSameOriginWith(GURL(it->second->sw_scope),
                                 web_app_manifest_url));

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

  std::move(callback_).Run(std::move(installable_apps_));
  std::move(finished_using_resources_).Run();
}

void InstallablePaymentAppCrawler::WarnIfPossible(const std::string& message) {
  if (web_contents()) {
    web_contents()->GetMainFrame()->AddMessageToConsole(
        content::ConsoleMessageLevel::CONSOLE_MESSAGE_LEVEL_WARNING, message);
  } else {
    LOG(WARNING) << message;
  }
}

}  // namespace payments.
