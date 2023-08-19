// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_DOWNLOADER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_DOWNLOADER_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

namespace web_app {

// Helper class to download the Signed Web Bundle of an Isolated Web App.
class IsolatedWebAppDownloader {
 public:
  using DownloadCallback = base::OnceCallback<void(int32_t net_error)>;

  // Creates a new instance of this class and starts the download process.
  static std::unique_ptr<IsolatedWebAppDownloader> CreateAndStartDownloading(
      GURL url,
      base::FilePath destination,
      net::PartialNetworkTrafficAnnotationTag partial_traffic_annotation,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      DownloadCallback download_callback);

  ~IsolatedWebAppDownloader();

 private:
  explicit IsolatedWebAppDownloader(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  void DownloadSignedWebBundle(
      GURL url,
      base::FilePath destination,
      net::PartialNetworkTrafficAnnotationTag partial_traffic_annotation,
      DownloadCallback download_callback);

  int32_t OnSignedWebBundleDownloaded(base::FilePath destination,
                                      base::FilePath actual_destination);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_DOWNLOADER_H_
