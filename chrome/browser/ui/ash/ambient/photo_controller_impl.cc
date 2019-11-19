// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/ambient/photo_controller_impl.h"

#include "ash/public/cpp/assistant/assistant_image_downloader.h"
#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/chromeos/backdrop_wallpaper_handlers/backdrop_wallpaper.pb.h"
#include "chrome/browser/chromeos/backdrop_wallpaper_handlers/backdrop_wallpaper_handlers.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/account_id/account_id.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/image/image_skia.h"

namespace {

// TODO(wutao): Discuss with UX if we should use pixel size and extract this
// helper function to be used by wallpaper_private_api.cc as well.
// Returns a suffix to be appended to the base url of Backdrop wallpapers.
std::string GetBackdropWallpaperSuffix() {
  // FIFE url is used for Backdrop wallpapers and the desired image size should
  // be specified. Currently we are using two times the display size. This is
  // determined by trial and error and is subject to change.
  gfx::Size display_size =
      display::Screen::GetScreen()->GetPrimaryDisplay().size();
  return "=w" + base::NumberToString(
                    2 * std::max(display_size.width(), display_size.height()));
}

std::string GetImageUrl(const backdrop::Image& image) {
  return image.image_url() + GetBackdropWallpaperSuffix();
}

}  // namespace

PhotoControllerImpl::PhotoControllerImpl() : weak_factory_(this) {}

PhotoControllerImpl::~PhotoControllerImpl() = default;

void PhotoControllerImpl::GetNextImage(PhotoDownloadCallback callback) {
  if (collections_list_.empty()) {
    GetCollectionsList(std::move(callback));
    return;
  }

  GetNextRandomImage(std::move(callback));
}

void PhotoControllerImpl::GetCollectionsList(PhotoDownloadCallback callback) {
  if (!collection_info_fetcher_) {
    collection_info_fetcher_ =
        std::make_unique<backdrop_wallpaper_handlers::CollectionInfoFetcher>();
  }
  // If this function is called consecutively, it is possible to invalidate the
  // previous callback. This is fine because we only need the latest collection
  // info.
  collection_info_fetcher_->Start(
      base::BindOnce(&PhotoControllerImpl::OnCollectionsInfoFetched,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void PhotoControllerImpl::OnCollectionsInfoFetched(
    PhotoDownloadCallback callback,
    bool success,
    const std::vector<backdrop::Collection>& collections_list) {
  if (!success || collections_list.empty()) {
    std::move(callback).Run(gfx::ImageSkia());
    return;
  }

  collections_list_ = collections_list;

  // TODO(wutao): Get |collection_id_| from settings. We need to handle the case
  // |collection_id_| is not in the latest list, e.g. user deleted some
  // collections but didn't update the settings on device.
  collection_id_ = collections_list_[0].collection_id();
  GetNextRandomImage(std::move(callback));
}

void PhotoControllerImpl::GetNextRandomImage(PhotoDownloadCallback callback) {
  // If this function is called consecutively, it is possible to invalidate the
  // previous request and callback and prevent fetching images in parallel.
  // TODO(wutao): Refactor SurpriseMeImageFetcher::Start to take |resume_token_|
  // in order not to create |surprise_me_image_fetcher_| every time.
  surprise_me_image_fetcher_ =
      std::make_unique<backdrop_wallpaper_handlers::SurpriseMeImageFetcher>(
          collection_id_, resume_token_);
  surprise_me_image_fetcher_->Start(
      base::BindOnce(&PhotoControllerImpl::OnNextRandomImageInfoFetched,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void PhotoControllerImpl::OnNextRandomImageInfoFetched(
    PhotoDownloadCallback callback,
    bool success,
    const backdrop::Image& image,
    const std::string& new_resume_token) {
  if (!success || image.image_url().empty()) {
    std::move(callback).Run(gfx::ImageSkia());
    return;
  }

  resume_token_ = new_resume_token;
  std::string image_url = GetImageUrl(image);
  AccountId account_id =
      chromeos::ProfileHelper::Get()
          ->GetUserByProfile(ProfileManager::GetActiveUserProfile())
          ->GetAccountId();
  ash::AssistantImageDownloader::GetInstance()->Download(
      account_id, GURL(image_url), std::move(callback));
}
