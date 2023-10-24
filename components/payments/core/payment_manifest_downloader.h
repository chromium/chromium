// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_PAYMENT_MANIFEST_DOWNLOADER_H_
#define COMPONENTS_PAYMENTS_CORE_PAYMENT_MANIFEST_DOWNLOADER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {
class HttpResponseHeaders;
struct RedirectInfo;
}  // namespace net

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace payments {

class CSPChecker;
class ErrorLogger;

// Called on completed download of a manifest |contents| from |url|, which is
// the final URL after following the redirects, if any.
//
// Download failure results in empty contents. Failure to download the manifest
// can happen because of the following reasons:
//  - HTTP response code is not 200. (204 is also allowed for payment method
//    manifest.)
//
// In the case of a payment method manifest download, can also fail when:
//  - More than three redirects.
//  - Cross-site redirects.
//  - HTTP GET on the manifest URL returns empty content and:
//      - HTTP response headers are absent.
//      - HTTP response headers do not contain Link headers.
//      - Link header does not contain rel="payment-method-manifest".
//      - Link header does not contain a valid URL of the same origin.
//  - After following the Link header:
//      - There's a redirect.
//      - HTTP GET returns empty content.
//
// In the case of a web app manifest download, can also also fail when:
//  - There's a redirect.
//  - HTTP GET on the manifest URL returns empty content.
using PaymentManifestDownloadCallback =
    base::OnceCallback<void(const GURL& url,
                            const std::string& contents,
                            const std::string& error_message)>;

// Downloader of the payment method manifest and web-app manifest based on the
// payment method name that is a URL with HTTPS scheme, e.g.,
// https://bobpay.com.
//
// The downloader follows up to three redirects for the payment method manifest
// request only. Three is enough for known legitimate use cases and seems like a
// good upper bound.
//
// The command line must be initialized to use this class in tests, because it
// checks for --unsafely-treat-insecure-origin-as-secure=<origin> flag. For
// example:
//  base::CommandLine::Init(0, nullptr);
class PaymentManifestDownloader {
 public:
  PaymentManifestDownloader(
      std::unique_ptr<ErrorLogger> log,
      base::WeakPtr<CSPChecker> csp_checker,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  PaymentManifestDownloader(const PaymentManifestDownloader&) = delete;
  PaymentManifestDownloader& operator=(const PaymentManifestDownloader&) =
      delete;

  virtual ~PaymentManifestDownloader();

  // Download a payment method manifest from |url| via a GET. The HTTP response
  // header is parsed for Link header. If there is no Link header, then the body
  // is returned. If there's a Link header, then it is followed exactly once.
  // Example header:
  //
  //      Link: <data/payment-manifest.json>; rel="payment-method-manifest"
  //
  // (This is relative to the payment method URL.) Example of an absolute
  // location:
  //
  //      Link: <https://bobpay.com/data/payment-manifest.json>;
  //      rel="payment-method-manifest"
  //
  // The absolute location must use HTTPS scheme.
  //
  // |merchant_origin| should be the origin of the iframe that created the
  // PaymentRequest object. It is used by security features like
  // 'Sec-Fetch-Site' and 'Cross-Origin-Resource-Policy'.
  //
  // |url| should be valid according to UrlUtil::IsValidManifestUrl() to
  // download.
  void DownloadPaymentMethodManifest(const url::Origin& merchant_origin,
                                     const GURL& url,
                                     PaymentManifestDownloadCallback callback);

  // Download a web app manifest from |url| via a single HTTP request:
  //
  // 1) GET request for the payment method name.
  //
  // |payment_method_manifest_origin| should be the origin of the payment method
  // manifest that is pointing to this web app manifest. It is used for security
  // features like 'Sec-Fetch-Site' and 'Cross-Origin-Resource-Policy'.
  //
  // |url| should be valid according to UrlUtil::IsValidManifestUrl() to
  // download.
  void DownloadWebAppManifest(const url::Origin& payment_method_manifest_origin,
                              const GURL& url,
                              PaymentManifestDownloadCallback callback);

  // Overridden in TestDownloader to convert |url| to a test server URL. The
  // default implementation here simply returns |url|.
  virtual GURL FindTestServerURL(const GURL& url) const;

  // Overridden in TestDownloader to allow modifying CSP. Should not be called
  // in production.
  virtual void SetCSPCheckerForTesting(base::WeakPtr<CSPChecker> csp_checker);

 private:
  friend class PaymentManifestDownloaderTestBase;
  friend class TestDownloader;

  // Information about an ongoing download request.
  struct Download {
    enum class Type {
      LINK_HEADER_WITH_FALLBACK_TO_RESPONSE_BODY,
      FALLBACK_TO_RESPONSE_BODY,
      RESPONSE_BODY,
    };

    Download();
    ~Download();

    // Returns true if this download is an HTTP HEAD request for a payment
    // manifest.
    bool IsLinkHeaderDownload() const;

    // Returns true if this download is an HTTP GET request either for payment
    // method manifest or for a web app manifest file.
    bool IsResponseBodyDownload() const;

    int allowed_number_of_redirects = 0;
    Type type = Type::RESPONSE_BODY;
    url::Origin request_initiator;
    GURL original_url;
    GURL url_before_redirects;
    bool did_follow_redirect = false;
    std::unique_ptr<network::SimpleURLLoader> loader;
    PaymentManifestDownloadCallback callback;
  };

  // Called by SimpleURLLoader on a redirect.
  void OnURLLoaderRedirect(network::SimpleURLLoader* url_loader,
                           const GURL& url_before_redirect,
                           const net::RedirectInfo& redirect_info,
                           const network::mojom::URLResponseHead& response_head,
                           std::vector<std::string>* to_be_removed_headers);

  // Called by SimpleURLLoader on completion.
  void OnURLLoaderComplete(network::SimpleURLLoader* url_loader,
                           std::unique_ptr<std::string> response_body);

  // Internally called by OnURLLoaderComplete, exposed to ease unit tests.
  void OnURLLoaderCompleteInternal(
      network::SimpleURLLoader* url_loader,
      const GURL& final_url,
      const std::string& response_body,
      scoped_refptr<net::HttpResponseHeaders> headers,
      int net_error);

  void TryFallbackToDownloadingResponseBody(
      const GURL& url_to_download,
      std::unique_ptr<Download> download_info);

  // Called by unittests to get the one in-progress loader.
  network::SimpleURLLoader* GetLoaderForTesting();

  // Called by unittests to get the original URL of the in-progress loader.
  GURL GetLoaderOriginalURLForTesting();

  // Overridden in TestDownloader.
  virtual void InitiateDownload(const url::Origin& request_initiator,
                                const GURL& url,
                                const GURL& url_before_redirects,
                                bool did_follow_redirect,
                                Download::Type download_type,
                                int allowed_number_of_redirects,
                                PaymentManifestDownloadCallback callback);

  void OnCSPCheck(std::unique_ptr<Download> download, bool csp_allowed);

  std::unique_ptr<ErrorLogger> log_;
  base::WeakPtr<CSPChecker> csp_checker_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Downloads are identified by network::SimpleURLLoader pointers, because
  // that's the only unique piece of information that OnURLLoaderComplete()
  // receives. Can't rely on the URL of the download, because of possible
  // collision between HEAD and GET requests.
  std::map<const network::SimpleURLLoader*, std::unique_ptr<Download>>
      downloads_;

  base::WeakPtrFactory<PaymentManifestDownloader> weak_ptr_factory_{this};
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_PAYMENT_MANIFEST_DOWNLOADER_H_
