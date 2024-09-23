// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_downloader.h"

#include <memory>
#include <optional>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/thread_pool.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace web_app {

void ScopedTempWebBundleFile::Create(
    base::OnceCallback<void(ScopedTempWebBundleFile)> callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce([]() -> ScopedTempWebBundleFile {
        auto file = std::make_unique<base::ScopedTempFile>();
        if (!file->Create()) {
          return ScopedTempWebBundleFile(/*file=*/nullptr);
        }
        return ScopedTempWebBundleFile(std::move(file));
      }),
      std::move(callback));
}

ScopedTempWebBundleFile::ScopedTempWebBundleFile(
    std::unique_ptr<base::ScopedTempFile> file)
    : file_(std::move(file)) {}

ScopedTempWebBundleFile::~ScopedTempWebBundleFile() {
  if (!file_) {
    return;
  }

  // Deleting the file must happen on a thread that allows blocking.
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(
          [](std::unique_ptr<base::ScopedTempFile> file) {
            // `file` is deleted here.
          },
          std::move(file_)));
}

ScopedTempWebBundleFile& ScopedTempWebBundleFile::operator=(
    ScopedTempWebBundleFile&&) = default;
ScopedTempWebBundleFile::ScopedTempWebBundleFile(ScopedTempWebBundleFile&&) =
    default;

const base::FilePath& ScopedTempWebBundleFile::path() const {
  CHECK(file_) << "`path()` must not be called on a nullptr `file_`.";
  return file_->path();
}

// static
std::unique_ptr<IsolatedWebAppDownloader>
IsolatedWebAppDownloader::CreateAndStartDownloading(
    GURL url,
    base::FilePath destination,
    net::PartialNetworkTrafficAnnotationTag partial_traffic_annotation,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    IsolatedWebAppDownloader::DownloadCallback download_callback) {
  auto downloader = base::WrapUnique(
      new IsolatedWebAppDownloader(std::move(url_loader_factory)));
  downloader->DownloadSignedWebBundle(std::move(url), std::move(destination),
                                      std::move(partial_traffic_annotation),
                                      std::move(download_callback));
  return downloader;
}

IsolatedWebAppDownloader::IsolatedWebAppDownloader(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(std::move(url_loader_factory)) {}

IsolatedWebAppDownloader::~IsolatedWebAppDownloader() = default;

void IsolatedWebAppDownloader::DownloadSignedWebBundle(
    GURL url,
    base::FilePath destination,
    net::PartialNetworkTrafficAnnotationTag partial_traffic_annotation,
    DownloadCallback download_callback) {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::CompleteNetworkTrafficAnnotation("iwa_bundle_downloader",
                                            partial_traffic_annotation,
                                            R"(
    semantics {
      data:
        "This request does not send any user data. Its destination is the URL "
        "of a Signed Web Bundle of an Isolated Web App that is installed for "
        "the user."
      destination: OTHER
      internal {
        contacts {
          owners: "//chrome/browser/web_applications/isolated_web_apps/OWNERS"
        }
      }
      user_data {
        type: NONE
      }
      last_reviewed: "2023-06-01"
    }
    policy {
      cookies_allowed: NO
    })");

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  // Cookies are not allowed.
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), std::move(traffic_annotation));

  simple_url_loader_->SetRetryOptions(
      /* max_retries=*/3,
      network::SimpleURLLoader::RETRY_ON_5XX |
          network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);

  simple_url_loader_->DownloadToFile(
      url_loader_factory_.get(),
      base::BindOnce(&IsolatedWebAppDownloader::OnSignedWebBundleDownloaded,
                     // The callback will never run if `this` is deleted,
                     // because `simple_url_loader_` is a member of `this`.
                     base::Unretained(this), destination)
          .Then(std::move(download_callback)),
      destination);
}

int32_t IsolatedWebAppDownloader::OnSignedWebBundleDownloaded(
    base::FilePath destination,
    base::FilePath actual_destination) {
  if (actual_destination.empty()) {
    int32_t net_error = simple_url_loader_->NetError();
    CHECK_NE(net_error, net::OK);
    return net_error;
  }
  CHECK_EQ(actual_destination, destination);
  return net::OK;
}

}  // namespace web_app
