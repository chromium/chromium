// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_TEST_PAYMENT_MANIFEST_DOWNLOADER_H_
#define COMPONENTS_PAYMENTS_CORE_TEST_PAYMENT_MANIFEST_DOWNLOADER_H_

#include <map>
#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "components/payments/core/payment_manifest_downloader.h"

class GURL;

template <class T>
class scoped_refptr;

namespace network {
class SharedURLLoaderFactory;
}

namespace payments {

class CSPChecker;

// Downloads payment method manifests from the test server.
//
// Sample usage #1:
//
//   TestDownloader downloader(csp_checker, url_loader_factory);
//   downloader.AddTestServerURL("https://", "https://127.0.0.1:7070");
//   // Actual URL downloaded is https://127.0.0.1:7070/alicepay.test/webpay.
//   downloader.DownloadPaymentMethodManifest(
//       "https://alicepay.test/webpay", callback);
//
// Sample usage #2:
//
//   TestDownloader downloader(csp_checker, url_loader_factory);
//   downloader.AddTestServerURL(
//       "https://alicepay.test", "https://127.0.0.1:8080");
//   downloader.AddTestServerURL(
//       "https://bobpay.test", "https://127.0.0.1:9090");
//   // Actual URL downloaded is https://127.0.0.1:8080/webpay.
//   downloader.DownloadPaymentMethodManifest(
//       "https://alicepay.test/webpay", callback);
//   // Actual URL downloaded is https://127.0.0.1:9090/webpay.
//   downloader.DownloadPaymentMethodManifest(
//       "https://bobpay.test/webpay", callback);
class TestDownloader : public PaymentManifestDownloader {
 public:
  TestDownloader(
      base::WeakPtr<CSPChecker> csp_checker,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  TestDownloader(const TestDownloader&) = delete;
  TestDownloader& operator=(const TestDownloader&) = delete;

  ~TestDownloader() override;

  // Modifies the downloader to replace all instances of |prefix| with
  // |test_server_url| when downloading payment method manifests and web app
  // manifests.
  //
  // For example, if AddTestServerURL("https://", "https://127.0.0.1:7070") is
  // called, then all calls to DownloadPaymentMethodManifest(some_url, callback)
  // will replace the "https://" prefix of some_url with
  // "https://127.0.0.1:7070". This is useful when running a single test server
  // that serves files in components/test/data/payments/, which has
  // subdirectories that look like hostnames. So, downloading
  // "https://alicepay.test/webpay" would actually download
  // https://127.0.0.1:7070/alicepay.test/webpay, which is a file located at
  // components/test/data/payments/alicepay.test/webpay.
  //
  // For another example, if AddTestServerURL("https://alicepay.test",
  // "https://127.0.0.1:8080") is called, then all calls to
  // DownloadPaymentMethodManifest(some_url, callback) will replace the
  // "https://alicepay.test" prefix of some_url with "https://127.0.0.1:8080".
  // This is useful when running multiple test servers, each one serving file
  // from individual subdirectories for components/test/data/payments/. So,
  // downloading "https://alicepay.test/webpay" would actually download
  // https://127.0.0.1:8080/webpay, which is a file located at
  // components/test/data/payments/alicepay.test/webpay. Multiple test servers
  // are useful for testing where the RFC6454 origins should be considered.
  //
  // Any call to DownloadPaymentMethodManifest(some_url, callback) where
  // some_url does not have a previously added prefix will use the original
  // some_url without modifications.
  //
  // If you call this method multiple times, avoid |prefix| parameters that are
  // prefixes of each other, as that will cause undefined confusion. That is,
  // AddTestServerURL("x");AddTestServerURL("y"); is OK, but
  // AddTestServerURL("x");AddTestServerURL("xy"); is not.
  void AddTestServerURL(const std::string& prefix, const GURL& test_server_url);

 private:
  // PaymentManifestDownloader:
  //
  // The reverse operation as AddTestServerURL: converts |url| back to a test
  // server URL so it can be fetched as a normal resource outside of this class.
  GURL FindTestServerURL(const GURL& url) const override;

  // PaymentManifestDownloader:
  //
  // Overrides the Content-Security-Policy (CSP) checker being used.
  void SetCSPCheckerForTesting(base::WeakPtr<CSPChecker> csp_checker) override;

  // PaymentManifestDownloader:
  //
  // Replaces the given URLs with the test server URLs before initiating
  // download.
  void InitiateDownload(const url::Origin& request_initiator,
                        const GURL& url,
                        const GURL& url_before_redirects,
                        bool did_follow_redirect,
                        Download::Type download_type,
                        int allowed_number_of_redirects,
                        PaymentManifestDownloadCallback callback) override;

  // The mapping from the URL prefix to the URL of the test server to be used.
  // Example 1:
  //
  // {"https://": "https://127.0.0.1:7070"}
  //
  // Example 2:
  //
  // {
  //   "https://alicepay.test": "https://127.0.0.1:8080",
  //   "https://bobpay.test": "https://127.0.0.1:9090"
  // }
  std::map<std::string, GURL> test_server_url_;

  base::WeakPtrFactory<TestDownloader> weak_ptr_factory_{this};
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_TEST_PAYMENT_MANIFEST_DOWNLOADER_H_
