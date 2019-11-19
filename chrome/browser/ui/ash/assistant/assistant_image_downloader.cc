// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/assistant/assistant_image_downloader.h"

#include "chrome/browser/bitmap_fetcher/bitmap_fetcher.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_delegate.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/load_flags.h"

namespace {

constexpr net::NetworkTrafficAnnotationTag kNetworkTrafficAnnotationTag =
    net::DefineNetworkTrafficAnnotation("assistant_image_downloader", R"(
          "semantics: {
            sender: "Google Assistant"
            description:
              "The Google Assistant requires dynamic loading of images to "
              "provide a media rich user experience. Images are downloaded on "
              "an as needed basis."
            trigger:
              "Generally triggered in direct response to a user issued query. "
              "A single query may necessitate the downloading of multiple "
              "images."
            destination: GOOGLE_OWNED_SERVICE
          }
          "policy": {
            cookies_allowed: NO
            setting:
              "The Google Assistant can be enabled/disabled in Chrome Settings "
              "and is subject to eligibility requirements."
          })");

// DownloadTask ----------------------------------------------------------------

class DownloadTask : public BitmapFetcherDelegate {
 public:
  DownloadTask(Profile* profile,
               const GURL& url,
               ash::AssistantImageDownloader::DownloadCallback callback)
      : callback_(std::move(callback)) {
    StartTask(profile, url);
  }

  ~DownloadTask() override = default;

  // BitmapFetcherDelegate:
  void OnFetchComplete(const GURL& url, const SkBitmap* bitmap) override {
    std::move(callback_).Run(bitmap
                                 ? gfx::ImageSkia::CreateFrom1xBitmap(*bitmap)
                                 : gfx::ImageSkia());
    delete this;
  }

 private:
  void StartTask(Profile* profile, const GURL& url) {
    bitmap_fetcher_ = std::make_unique<BitmapFetcher>(
        url, this, kNetworkTrafficAnnotationTag);

    bitmap_fetcher_->Init(
        /*referrer=*/std::string(), net::URLRequest::NEVER_CLEAR_REFERRER,
        network::mojom::CredentialsMode::kOmit);

    bitmap_fetcher_->Start(
        content::BrowserContext::GetDefaultStoragePartition(profile)
            ->GetURLLoaderFactoryForBrowserProcess()
            .get());
  }

  ash::AssistantImageDownloader::DownloadCallback callback_;
  std::unique_ptr<BitmapFetcher> bitmap_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(DownloadTask);
};

}  // namespace

// AssistantImageDownloader ----------------------------------------------------

AssistantImageDownloader::AssistantImageDownloader() = default;

AssistantImageDownloader::~AssistantImageDownloader() = default;

void AssistantImageDownloader::Download(
    const AccountId& account_id,
    const GURL& url,
    ash::AssistantImageDownloader::DownloadCallback callback) {
  Profile* profile =
      chromeos::ProfileHelper::Get()->GetProfileByAccountId(account_id);

  if (!profile) {
    LOG(WARNING) << "Unable to retrieve profile for account_id.";
    std::move(callback).Run(gfx::ImageSkia());
    return;
  }

  // The download task will delete itself upon task completion.
  new DownloadTask(profile, url, std::move(callback));
}
