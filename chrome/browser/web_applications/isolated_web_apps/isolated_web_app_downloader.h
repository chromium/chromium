// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_DOWNLOADER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_DOWNLOADER_H_

#include <memory>
#include <optional>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_file.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

namespace web_app {

class ScopedTempWebBundleFile {
 public:
  // Creates a ScopedTempWebBundleFile on a non-blocking thread.
  // The result might be null if something goes wrong during the operation.
  static void Create(
      base::OnceCallback<void(ScopedTempWebBundleFile)> callback);

  explicit ScopedTempWebBundleFile(
      std::unique_ptr<base::ScopedTempFile> file = nullptr);

  // `file_` is deleted on a non-blocking thread.
  ~ScopedTempWebBundleFile();

  ScopedTempWebBundleFile& operator=(const ScopedTempWebBundleFile&) = delete;
  ScopedTempWebBundleFile(const ScopedTempWebBundleFile&) = delete;

  ScopedTempWebBundleFile& operator=(ScopedTempWebBundleFile&&);
  ScopedTempWebBundleFile(ScopedTempWebBundleFile&&);

  explicit operator bool() const { return !!file_; }

  const base::ScopedTempFile* file() const { return file_.get(); }

  // Will CHECK() if `file_` is nullptr.
  const base::FilePath& path() const;

 private:
  std::unique_ptr<base::ScopedTempFile> file_;
};

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

  explicit IsolatedWebAppDownloader(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~IsolatedWebAppDownloader();

  void DownloadSignedWebBundle(
      GURL url,
      base::FilePath destination,
      net::PartialNetworkTrafficAnnotationTag partial_traffic_annotation,
      DownloadCallback download_callback);

 private:
  int32_t OnSignedWebBundleDownloaded(base::FilePath destination,
                                      base::FilePath actual_destination);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_DOWNLOADER_H_
