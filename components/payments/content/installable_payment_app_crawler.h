// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_INSTALLABLE_PAYMENT_APP_CRAWLER_H_
#define COMPONENTS_PAYMENTS_CONTENT_INSTALLABLE_PAYMENT_APP_CRAWLER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/payments/content/developer_console_logger.h"
#include "components/payments/content/manifest_verifier.h"
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/payments/content/utility/payment_manifest_parser.h"
#include "components/payments/content/web_app_manifest.h"
#include "components/payments/core/payment_manifest_downloader.h"
#include "content/public/browser/global_routing_id.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"
#include "url/origin.h"

class GURL;

namespace content {
class RenderFrameHost;
}  // namespace content

namespace payments {

struct RefetchedIcon {
  RefetchedIcon();
  ~RefetchedIcon();
  std::string method_name;
  std::unique_ptr<SkBitmap> icon;
};

// Crawls installable web payment apps. First, fetches and parses the payment
// method manifests to get 'default_applications' manifest urls. Then, fetches
// and parses the web app manifests to get the installable payment apps' info.
class InstallablePaymentAppCrawler {
 public:
  using FinishedCrawlingCallback = base::OnceCallback<void(
      std::map<GURL, std::unique_ptr<WebAppInstallationInfo>>,
      std::map<GURL, std::unique_ptr<RefetchedIcon>>,
      const std::string& error_message)>;

  enum class CrawlingMode {
    // In this mode the crawler will crawl for finding JIT installable payment
    // apps.
    kJustInTimeInstallation,
    // In this mode the crawler will crawl for downloading missing icons for
    // already installed payment apps.
    kMissingIconRefetch,
  };

  // |merchant_origin| is the origin of the iframe that created the
  // PaymentRequest object. It is used by security features like
  // 'Sec-Fetch-Site' and 'Cross-Origin-Resource-Policy'.
  // |initiator_render_frame_host| is the iframe for |merchant_origin|.
  //
  // The owner of InstallablePaymentAppCrawler owns |downloader|, |parser| and
  // |cache|. They should live until |finished_using_resources| parameter to
  // Start() method is called.
  InstallablePaymentAppCrawler(
      const url::Origin& merchant_origin,
      content::RenderFrameHost* initiator_render_frame_host,
      PaymentManifestDownloader* downloader,
      PaymentManifestParser* parser,
      PaymentManifestWebDataService* cache);

  InstallablePaymentAppCrawler(const InstallablePaymentAppCrawler&) = delete;
  InstallablePaymentAppCrawler& operator=(const InstallablePaymentAppCrawler&) =
      delete;

  ~InstallablePaymentAppCrawler();

  // Starts the crawling process. All the url based payment methods in
  // |request_method_data| will be crawled. A list of installable payment apps'
  // info will be send back through |callback|. |finished_using_resources| is
  // called after finished using the resources (downloader, parser and cache),
  // then this object is safe to be deleted.
  void Start(
      const std::vector<mojom::PaymentMethodDataPtr>& requested_method_data,
      std::set<GURL> method_manifest_urls_for_icon_refetch,
      FinishedCrawlingCallback callback,
      base::OnceClosure finished_using_resources);

  void IgnorePortInOriginComparisonForTesting();

  bool IsSameOriginWith(const GURL& a, const GURL& b);

 private:
  void OnPaymentMethodManifestDownloaded(
      const GURL& method_manifest_url,
      const GURL& method_manifest_url_after_redirects,
      const std::string& content,
      const std::string& error_message);
  void OnPaymentMethodManifestParsed(
      const GURL& method_manifest_url,
      const GURL& method_manifest_url_after_redirects,
      const std::string& content,
      const std::vector<GURL>& default_applications,
      const std::vector<url::Origin>& supported_origins);
  void OnPaymentWebAppManifestDownloaded(
      const GURL& method_manifest_url,
      const GURL& web_app_manifest_url,
      const GURL& web_app_manifest_url_after_redirects,
      const std::string& content,
      const std::string& error_message);
  void OnPaymentWebAppInstallationInfo(
      const GURL& method_manifest_url,
      const GURL& web_app_manifest_url,
      std::unique_ptr<WebAppInstallationInfo> app_info,
      std::unique_ptr<std::vector<PaymentManifestParser::WebAppIcon>> icons);
  bool CompleteAndStorePaymentWebAppInfoIfValid(
      const GURL& method_manifest_url,
      const GURL& web_app_manifest_url,
      std::unique_ptr<WebAppInstallationInfo> app_info);
  // Returns true if an icon can get downloaded for the web app with the given
  // manifest.
  bool DownloadAndDecodeWebAppIcon(
      const GURL& method_manifest_url,
      const GURL& web_app_manifest_url,
      std::unique_ptr<std::vector<PaymentManifestParser::WebAppIcon>> icons);
  void OnPaymentWebAppIconDownloadAndDecoded(const GURL& method_manifest_url,
                                             const GURL& web_app_manifest_url,
                                             const SkBitmap& icon);
  void PostTaskToFinishCrawlingPaymentAppsIfReady();
  void FinishCrawlingPaymentAppsIfReady();
  void SetFirstError(const std::string& error_message);

  DeveloperConsoleLogger log_;
  const url::Origin merchant_origin_;
  const content::GlobalRenderFrameHostId initiator_frame_routing_id_;
  raw_ptr<PaymentManifestDownloader> downloader_;
  raw_ptr<PaymentManifestParser> parser_;
  FinishedCrawlingCallback callback_;
  base::OnceClosure finished_using_resources_;

  size_t number_of_payment_method_manifest_to_download_;
  size_t number_of_payment_method_manifest_to_parse_;
  size_t number_of_web_app_manifest_to_download_;
  size_t number_of_web_app_manifest_to_parse_;
  size_t number_of_web_app_icons_to_download_and_decode_;
  std::set<GURL> downloaded_web_app_manifests_;
  std::map<GURL, std::unique_ptr<WebAppInstallationInfo>> installable_apps_;
  std::map<GURL, std::unique_ptr<RefetchedIcon>> refetched_icons_;
  std::set<GURL> method_manifest_urls_for_icon_refetch_;

  // The first error message (if any) to be forwarded to the merchant when
  // rejecting the promise returned from PaymentRequest.show().
  std::string first_error_message_;

  bool ignore_port_in_origin_comparison_for_testing_ = false;

  CrawlingMode crawling_mode_ = CrawlingMode::kJustInTimeInstallation;

  base::WeakPtrFactory<InstallablePaymentAppCrawler> weak_ptr_factory_{this};
};

}  // namespace payments.

#endif  // COMPONENTS_PAYMENTS_CONTENT_INSTALLABLE_PAYMENT_APP_CRAWLER_H_
