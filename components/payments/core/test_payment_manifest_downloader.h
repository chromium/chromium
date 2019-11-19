// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_TEST_PAYMENT_MANIFEST_DOWNLOADER_H_
#define COMPONENTS_PAYMENTS_CORE_TEST_PAYMENT_MANIFEST_DOWNLOADER_H_

#include <map>
#include <memory>
#include <string>

#include "base/macros.h"
#include "components/payments/core/payment_manifest_downloader.h"

class GURL;

template <class T>
class scoped_refptr;

namespace network {
class SharedURLLoaderFactory;
}

namespace payments {

// Downloads payment method manifests from the test server.
//
// Sample usage #1:
//
//   TestDownloader downloader(context);
//   downloader.AddTestServerURL("https://", "https://127.0.0.1:7070");
//   // Actual URL downloaded is https://127.0.0.1:7070/alicepay.com/webpay.
//   downloader.DownloadPaymentMethodManifest(
//       "https://alicepay.com/webpay", callback);
//
// Sample usage #2:
//
//   TestDownloader downloader(context);
//   downloader.AddTestServerURL(
//       "https://alicepay.com", "https://127.0.0.1:8080");
//   downloader.AddTestServerURL(
//       "https://bobpay.com", "https://127.0.0.1:9090");
//   // Actual URL downloaded is https://127.0.0.1:8080/webpay.
//   downloader.DownloadPaymentMethodManifest(
//       "https://alicepay.com/webpay", callback);
//   // Actual URL downloaded is https://127.0.0.1:9090/webpay.
//   downloader.DownloadPaymentMethodManifest(
//       "https://bobpay.com/webpay", callback);
class TestDownloader : public PaymentManifestDownloader {
 public:
  explicit TestDownloader(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
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
  // "https://alicepay.com/webpay" would actually download
  // https://127.0.0.1:7070/alicepay.com/webpay, which is a file located at
  // components/test/data/payments/alicepay.com/webpay.
  //
  // For anoter example, if AddTestServerURL("https://alicepay.com",
  // "https://127.0.0.1:8080") is called, then all calls to
  // DownloadPaymentMethodManifest(some_url, callback) will replace the
  // "https://alicepay.com" prefix of some_url with "https://127.0.0.1:8080".
  // This is useful when running multiple test servers, each one serving file
  // from individual subdirectories for components/test/data/payments/. So,
  // downloading "https://alicepay.com/webpay" would actually download
  // https://127.0.0.1:8080/webpay, which is a file located at
  // components/test/data/payments/alicepay.com/webpay. Multiple test servers
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

  // PaymentManifestDownloader:
  //
  // The reverse operation as AddTestServerURL: converts |url| back to a test
  // server URL so it can be fetched as a normal resource outside of this class.
  GURL FindTestServerURL(const GURL& url) const override;

 private:
  // PaymentManifestDownloader implementation.
  void InitiateDownload(const GURL& url,
                        const std::string& method,
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
  //   "https://alicepay.com": "https://127.0.0.1:8080",
  //   "https://bobpay.com": "https://127.0.0.1:9090"
  // }
  std::map<std::string, GURL> test_server_url_;

  DISALLOW_COPY_AND_ASSIGN(TestDownloader);
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_TEST_PAYMENT_MANIFEST_DOWNLOADER_H_
