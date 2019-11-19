// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_INSTALLABLE_PAYMENT_APP_CRAWLER_H_
#define COMPONENTS_PAYMENTS_CONTENT_INSTALLABLE_PAYMENT_APP_CRAWLER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/payments/content/developer_console_logger.h"
#include "components/payments/content/manifest_verifier.h"
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/payments/content/utility/payment_manifest_parser.h"
#include "components/payments/content/web_app_manifest.h"
#include "components/payments/core/payment_manifest_downloader.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"

class GURL;

namespace content {
class WebContents;
}

namespace payments {

// Crawls installable web payment apps. First, fetches and parses the payment
// method manifests to get 'default_applications' manifest urls. Then, fetches
// and parses the web app manifests to get the installable payment apps' info.
class InstallablePaymentAppCrawler : public content::WebContentsObserver {
 public:
  using FinishedCrawlingCallback = base::OnceCallback<void(
      std::map<GURL, std::unique_ptr<WebAppInstallationInfo>>,
      const std::string& error_message)>;

  // The owner of InstallablePaymentAppCrawler owns |downloader|, |parser| and
  // |cache|. They should live until |finished_using_resources| parameter to
  // Start() method is called.
  InstallablePaymentAppCrawler(content::WebContents* web_contents,
                               PaymentManifestDownloader* downloader,
                               PaymentManifestParser* parser,
                               PaymentManifestWebDataService* cache);
  ~InstallablePaymentAppCrawler() override;

  // Starts the crawling process. All the url based payment methods in
  // |request_method_data| will be crawled. A list of installable payment apps'
  // info will be send back through |callback|. |finished_using_resources| is
  // called after finished using the resources (downloader, parser and cache),
  // then this object is safe to be deleted.
  void Start(
      const std::vector<mojom::PaymentMethodDataPtr>& requested_method_data,
      FinishedCrawlingCallback callback,
      base::OnceClosure finished_using_resources);

  void IgnorePortInOriginComparisonForTesting();

 private:
  bool IsSameOriginWith(const GURL& a, const GURL& b);

  void OnPaymentMethodManifestDownloaded(
      const GURL& method_manifest_url,
      const GURL& method_manifest_url_after_redirects,
      const std::string& content,
      const std::string& error_message);
  void OnPaymentMethodManifestParsed(
      const GURL& method_manifest_url,
      const GURL& method_manifest_url_after_redirects,
      const std::vector<GURL>& default_applications,
      const std::vector<url::Origin>& supported_origins,
      bool all_origins_supported);
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
  void DownloadAndDecodeWebAppIcon(
      const GURL& method_manifest_url,
      const GURL& web_app_manifest_url,
      std::unique_ptr<std::vector<PaymentManifestParser::WebAppIcon>> icons);
  void OnPaymentWebAppIconDownloadAndDecoded(const GURL& method_manifest_url,
                                             const GURL& web_app_manifest_url,
                                             const SkBitmap& icon);
  void FinishCrawlingPaymentAppsIfReady();
  void SetFirstError(const std::string& error_message);

  DeveloperConsoleLogger log_;
  PaymentManifestDownloader* downloader_;
  PaymentManifestParser* parser_;
  FinishedCrawlingCallback callback_;
  base::OnceClosure finished_using_resources_;

  size_t number_of_payment_method_manifest_to_download_;
  size_t number_of_payment_method_manifest_to_parse_;
  size_t number_of_web_app_manifest_to_download_;
  size_t number_of_web_app_manifest_to_parse_;
  size_t number_of_web_app_icons_to_download_and_decode_;
  std::set<GURL> downloaded_web_app_manifests_;
  std::map<GURL, std::unique_ptr<WebAppInstallationInfo>> installable_apps_;

  // The first error message (if any) to be forwarded to the merchant when
  // rejecting the promise returned from PaymentRequest.show().
  std::string first_error_message_;

  bool ignore_port_in_origin_comparison_for_testing_ = false;

  base::WeakPtrFactory<InstallablePaymentAppCrawler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(InstallablePaymentAppCrawler);
};

}  // namespace payments.

#endif  // COMPONENTS_PAYMENTS_CONTENT_INSTALLABLE_PAYMENT_APP_CRAWLER_H_
