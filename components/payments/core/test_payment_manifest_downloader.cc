// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/test_payment_manifest_downloader.h"

#include <utility>

#include "base/memory/ref_counted.h"
#include "base/strings/string_util.h"
#include "components/payments/core/error_logger.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace payments {

TestDownloader::TestDownloader(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : PaymentManifestDownloader(std::make_unique<ErrorLogger>(),
                                url_loader_factory) {}

TestDownloader::~TestDownloader() {}

void TestDownloader::AddTestServerURL(const std::string& prefix,
                                      const GURL& test_server_url) {
  test_server_url_[prefix] = test_server_url;
}

void TestDownloader::InitiateDownload(
    const url::Origin& request_initiator,
    const GURL& url,
    Download::Type download_type,
    int allowed_number_of_redirects,
    PaymentManifestDownloadCallback callback) {
  PaymentManifestDownloader::InitiateDownload(
      request_initiator, FindTestServerURL(url), download_type,
      allowed_number_of_redirects, std::move(callback));
}

GURL TestDownloader::FindTestServerURL(const GURL& url) const {
  // Find the first key in |test_server_url_| that is a prefix of |url|. If
  // found, then replace this prefix in the |url| with the URL of the test
  // server that should be used instead.
  for (const auto& prefix_and_url : test_server_url_) {
    const std::string& prefix = prefix_and_url.first;
    const GURL& test_server_url = prefix_and_url.second;
    if (base::StartsWith(url.spec(), prefix, base::CompareCase::SENSITIVE) &&
        !base::StartsWith(url.spec(), test_server_url.spec(),
                          base::CompareCase::SENSITIVE)) {
      return GURL(test_server_url.spec() + url.spec().substr(prefix.length()));
    }
  }

  return url;
}

}  // namespace payments
