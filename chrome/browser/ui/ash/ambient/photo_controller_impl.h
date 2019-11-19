// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_AMBIENT_PHOTO_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_ASH_AMBIENT_PHOTO_CONTROLLER_IMPL_H_

#include <string>
#include <vector>

#include "ash/public/cpp/ambient/photo_controller.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"

namespace backdrop {
class Collection;
class Image;
}  // namespace backdrop

namespace backdrop_wallpaper_handlers {
class CollectionInfoFetcher;
class SurpriseMeImageFetcher;
}  // namespace backdrop_wallpaper_handlers

// TODO(wutao): Move this class to ash.
// Class to handle photos from Backdrop service.
class PhotoControllerImpl : public ash::PhotoController {
 public:
  using PhotoDownloadCallback = ash::PhotoController::PhotoDownloadCallback;

  PhotoControllerImpl();
  ~PhotoControllerImpl() override;

  // ash::PhotoController:
  void GetNextImage(PhotoDownloadCallback callback) override;

 private:
  void GetCollectionsList(PhotoDownloadCallback callback);
  void OnCollectionsInfoFetched(
      PhotoDownloadCallback callback,
      bool success,
      const std::vector<backdrop::Collection>& collections_list);
  void GetNextRandomImage(PhotoDownloadCallback callback);
  void OnNextRandomImageInfoFetched(PhotoDownloadCallback callback,
                                    bool success,
                                    const backdrop::Image& image,
                                    const std::string& new_resume_token);

  std::vector<backdrop::Collection> collections_list_;
  std::string collection_id_;
  std::string resume_token_;

  std::unique_ptr<backdrop_wallpaper_handlers::CollectionInfoFetcher>
      collection_info_fetcher_;
  std::unique_ptr<backdrop_wallpaper_handlers::SurpriseMeImageFetcher>
      surprise_me_image_fetcher_;

  base::WeakPtrFactory<PhotoControllerImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PhotoControllerImpl);
};

#endif  // CHROME_BROWSER_UI_ASH_AMBIENT_PHOTO_CONTROLLER_IMPL_H_
