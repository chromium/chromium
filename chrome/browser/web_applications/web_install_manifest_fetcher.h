// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_INSTALL_MANIFEST_FETCHER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_INSTALL_MANIFEST_FETCHER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/auto_reset.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "chrome/browser/web_applications/model/web_install_manifest_fetch_error.h"
#include "url/gurl.h"

namespace net {
struct RedirectInfo;
}  // namespace net

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
namespace mojom {
class URLResponseHead;
}  // namespace mojom
}  // namespace network

namespace web_app {

// Fetches a web app manifest from a URL using SimpleURLLoader. Used for a
// web-initiated manifest-first install flow to retrieve manifest JSON from the
// browser process. Does not parse the manifest — returns raw content on
// success.
class WebInstallManifestFetcher {
 public:
  using FetchCallback = base::OnceCallback<void(
      base::expected<std::string, WebInstallManifestFetchError>)>;

  WebInstallManifestFetcher(
      GURL manifest_url,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  ~WebInstallManifestFetcher();

  WebInstallManifestFetcher(const WebInstallManifestFetcher&) = delete;
  WebInstallManifestFetcher& operator=(const WebInstallManifestFetcher&) =
      delete;

  // Starts downloading. Will `CHECK` if called more than once.
  void Fetch(FetchCallback callback);

  // Overrides the max manifest download size for testing. The override is
  // automatically reverted when the returned AutoReset goes out of scope.
  static base::AutoReset<size_t> SetMaxManifestLengthForTesting(
      size_t max_length);

 private:
  void OnRedirect(const GURL& url_before_redirect,
                  const net::RedirectInfo& redirect_info,
                  const network::mojom::URLResponseHead& response_head,
                  std::vector<std::string>* removed_headers);
  void OnManifestDownloaded(std::optional<std::string> manifest_content);

  GURL manifest_url_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  FetchCallback fetch_callback_;
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;

  base::WeakPtrFactory<WebInstallManifestFetcher> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_INSTALL_MANIFEST_FETCHER_H_
