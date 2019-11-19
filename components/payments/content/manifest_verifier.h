// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_MANIFEST_VERIFIER_H_
#define COMPONENTS_PAYMENTS_CONTENT_MANIFEST_VERIFIER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/payments/content/developer_console_logger.h"
#include "components/webdata/common/web_data_service_base.h"
#include "components/webdata/common/web_data_service_consumer.h"
#include "content/public/browser/payment_app_provider.h"
#include "content/public/browser/web_contents_observer.h"

class GURL;

namespace content {
class WebContents;
}

namespace url {
class Origin;
}

namespace payments {

class PaymentManifestDownloader;
class PaymentManifestParser;
class PaymentManifestWebDataService;

// Verifies that payment handlers (i.e., service worker payment apps) can use
// the payment method names that they claim. Each object can be used to verify
// only one set of payment handlers.
//
// Sample usage:
//
//   ManifestVerifer verifier(std::move(downloader), std::move(parser), cache);
//   verifier.Verify(std::move(apps), base::BindOnce(&OnFinishedVerification),
//                   base::BindOnce(&OnFinishedUsingResources));
//
// The command line must be initialized to use this class in tests, because it
// checks for --unsafely-treat-insecure-origin-as-secure=<origin> flag. For
// example:
//  base::CommandLine::Init(0, nullptr);
class ManifestVerifier final : public WebDataServiceConsumer {
 public:
  // The callback that will be invoked with the validated payment handlers.
  // These payment handlers will have only the valid payment method names
  // remaining. If a payment handler does not have any valid payment method
  // names, then it's not included in the returned set of handlers.
  using VerifyCallback =
      base::OnceCallback<void(content::PaymentAppProvider::PaymentApps,
                              const std::string& error_message)>;

  // Creates the verifier and starts up the parser utility process.
  // The owner of ManifestVerifier owns |downloader|, |parser| and
  // |cache|. They should live until |finished_using_resources| parameter to
  // Verify() method is called.
  ManifestVerifier(content::WebContents* web_contents,
                   PaymentManifestDownloader* downloader,
                   PaymentManifestParser* parser,
                   PaymentManifestWebDataService* cache);

  ~ManifestVerifier() override;

  // Initiates the verification. This object should be deleted after
  // |finished_using_resources| is invoked.
  void Verify(content::PaymentAppProvider::PaymentApps apps,
              VerifyCallback finished_verification,
              base::OnceClosure finished_using_resources);

 private:
  // Called when a manifest is retrieved from cache.
  void OnWebDataServiceRequestDone(
      WebDataServiceBase::Handle h,
      std::unique_ptr<WDTypedResult> result) override;

  // Called when a manifest is downloaded. The "method manifest URL after
  // redirects" is intentionally not used.
  void OnPaymentMethodManifestDownloaded(
      const GURL& method_manifest_url,
      const GURL& unused_method_manifest_url_after_redirects,
      const std::string& content,
      const std::string& error_message);

  // Called when a manifest is parsed.
  void OnPaymentMethodManifestParsed(
      const GURL& method_manifest_url,
      const std::vector<GURL>& default_applications,
      const std::vector<url::Origin>& supported_origins,
      bool all_origins_supported);

  // Called immediately preceding the verification callback invocation.
  void RemoveInvalidPaymentApps();

  DeveloperConsoleLogger log_;

  // Downloads the manifests.
  PaymentManifestDownloader* downloader_;

  // Parses the manifests.
  PaymentManifestParser* parser_;

  // Caches the manifests.
  PaymentManifestWebDataService* cache_;

  // The list of payment apps being verified.
  content::PaymentAppProvider::PaymentApps apps_;

  // A mapping from the payment app scope to the set of the URL-based payment
  // methods that it claims to support, but is not allowed due to the payment
  // manifest contents.
  std::map<GURL, std::set<GURL>> prohibited_payment_methods_;

  // The callback to invoke when the verification completes.
  VerifyCallback finished_verification_callback_;

  // The callback to invoke after updating the manifest values in the on-disk
  // cache.
  base::OnceClosure finished_using_resources_callback_;

  // The mapping of payment method names to the origins of the apps that want to
  // use these payment method names.
  std::map<GURL, std::set<url::Origin>> manifest_url_to_app_origins_map_;

  // The mapping of cache request handles to the payment method manifest URLs.
  std::map<WebDataServiceBase::Handle, GURL> cache_request_handles_;

  // The set of payment method manifest URLs for which the cached value was
  // used.
  std::set<GURL> cached_manifest_urls_;

  // The mapping of payment method names to cached native payment app Ids.
  // Note that the supported native payment app Ids have been cached for a
  // payment method in the same cache in PaymentManifestVerifier.java. Do not
  // override them since we do not refresh them in this class.
  std::map<GURL, std::vector<std::string>> cached_supported_native_app_ids_;

  // The number of manifests that have not been verified yet. A manifest can be
  // either be retrieved from cache or downloaded for verification. Once this
  // number reaches 0, the verification callback is invoked.
  size_t number_of_manifests_to_verify_;

  // The number of manifests that have not been downloaded yet. Downloaded
  // manifests will be stored in cache and possibly will be used for caching.
  // Once this number reaches 0, the resource usage callback is invoked.
  size_t number_of_manifests_to_download_;

  // The first error message (if any) to be forwarded to the merchant when
  // rejecting the promise returned from PaymentRequest.show().
  std::string first_error_message_;

  base::WeakPtrFactory<ManifestVerifier> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ManifestVerifier);
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_MANIFEST_VERIFIER_H_
