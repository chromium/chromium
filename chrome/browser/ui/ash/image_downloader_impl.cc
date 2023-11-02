// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/image_downloader_impl.h"

#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/load_flags.h"
#include "net/url_request/referrer_policy.h"

namespace {

Profile* GetProfileForActiveUser() {
  const user_manager::User* const active_user =
      user_manager::UserManager::Get()->GetActiveUser();
  DCHECK(active_user);

  return ash::ProfileHelper::Get()->GetProfileByUser(active_user);
}

// DownloadTask ----------------------------------------------------------------

class DownloadTask : public BitmapFetcherDelegate {
 public:
  DownloadTask(const GURL& url,
               const net::NetworkTrafficAnnotationTag& annotation_tag,
               const net::HttpRequestHeaders& additional_headers,
               absl::optional<AccountId> credentials_account_id,
               ash::ImageDownloader::DownloadCallback callback)
      : callback_(std::move(callback)) {
    StartTask(url, annotation_tag, additional_headers, credentials_account_id);
  }

  DownloadTask(const DownloadTask&) = delete;
  DownloadTask& operator=(const DownloadTask&) = delete;
  ~DownloadTask() override = default;

  // BitmapFetcherDelegate:
  void OnFetchComplete(const GURL& url, const SkBitmap* bitmap) override {
    std::move(callback_).Run(bitmap
                                 ? gfx::ImageSkia::CreateFrom1xBitmap(*bitmap)
                                 : gfx::ImageSkia());
    delete this;
  }

 private:
  void StartTask(const GURL& url,
                 const net::NetworkTrafficAnnotationTag& annotation_tag,
                 const net::HttpRequestHeaders& additional_headers,
                 absl::optional<AccountId> credentials_account_id) {
    Profile* profile;
    if (credentials_account_id.has_value()) {
      profile = ash::ProfileHelper::Get()->GetProfileByAccountId(
          credentials_account_id.value());
    } else {
      profile = GetProfileForActiveUser();
    }
    if (!profile) {
      std::move(callback_).Run(gfx::ImageSkia());
      return;
    }

    bitmap_fetcher_ =
        std::make_unique<BitmapFetcher>(url, this, annotation_tag);

    auto credentials_mode = credentials_account_id.has_value()
                                ? network::mojom::CredentialsMode::kInclude
                                : network::mojom::CredentialsMode::kOmit;

    bitmap_fetcher_->Init(net::ReferrerPolicy::NEVER_CLEAR, credentials_mode,
                          additional_headers);

    bitmap_fetcher_->Start(profile->GetURLLoaderFactory().get());
  }

  ash::ImageDownloader::DownloadCallback callback_;
  std::unique_ptr<BitmapFetcher> bitmap_fetcher_;
};

}  // namespace

// ImageDownloaderImpl ----------------------------------------------------

ImageDownloaderImpl::ImageDownloaderImpl() = default;

ImageDownloaderImpl::~ImageDownloaderImpl() = default;

void ImageDownloaderImpl::Download(
    const GURL& url,
    const net::NetworkTrafficAnnotationTag& annotation_tag,
    ash::ImageDownloader::DownloadCallback callback) {
  Download(url, annotation_tag, /*additional_headers=*/{},
           /*credentials_account_id=*/absl::nullopt, std::move(callback));
}

void ImageDownloaderImpl::Download(
    const GURL& url,
    const net::NetworkTrafficAnnotationTag& annotation_tag,
    const net::HttpRequestHeaders& additional_headers,
    absl::optional<AccountId> credentials_account_id,
    ash::ImageDownloader::DownloadCallback callback) {
  // The download task will delete itself upon task completion.
  new DownloadTask(url, annotation_tag, additional_headers,
                   credentials_account_id, std::move(callback));
}
