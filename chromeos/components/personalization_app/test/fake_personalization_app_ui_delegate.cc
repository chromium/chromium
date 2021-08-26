// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/personalization_app/test/fake_personalization_app_ui_delegate.h"

#include <stdint.h>
#include <vector>

#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "base/check_op.h"
#include "base/unguessable_token.h"
#include "chromeos/components/personalization_app/mojom/personalization_app.mojom-forward.h"
#include "chromeos/components/personalization_app/mojom/personalization_app.mojom.h"
#include "chromeos/components/personalization_app/proto/backdrop_wallpaper.pb.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace {
const char kFakeCollectionId[] = "fake_collection_id";
}  // namespace

FakePersonalizationAppUiDelegate::FakePersonalizationAppUiDelegate(
    content::WebUI* web_ui) {}

FakePersonalizationAppUiDelegate::~FakePersonalizationAppUiDelegate() = default;

void FakePersonalizationAppUiDelegate::BindInterface(
    mojo::PendingReceiver<
        chromeos::personalization_app::mojom::WallpaperProvider> receiver) {
  wallpaper_receiver_.reset();
  wallpaper_receiver_.Bind(std::move(receiver));
}

void FakePersonalizationAppUiDelegate::FetchCollections(
    FetchCollectionsCallback callback) {
  std::vector<backdrop::Collection> collections;
  backdrop::Collection collection;
  collection.set_collection_id(kFakeCollectionId);
  collection.set_collection_name("Test Collection");
  backdrop::Image* image = collection.add_preview();
  image->set_asset_id(1);
  image->set_image_url(std::string());
  collections.push_back(collection);
  std::move(callback).Run(std::move(collections));
}

void FakePersonalizationAppUiDelegate::FetchImagesForCollection(
    const std::string& collection_id,
    FetchImagesForCollectionCallback callback) {
  DCHECK_EQ(collection_id, kFakeCollectionId);
  std::vector<backdrop::Image> images;
  backdrop::Image image;
  image.set_asset_id(1);
  image.set_image_url(std::string());
  image.add_attribution()->set_text("test");
  images.push_back(image);
  std::move(callback).Run(std::move(images));
}

void FakePersonalizationAppUiDelegate::GetLocalImages(
    GetLocalImagesCallback callback) {
  std::move(callback).Run({});
}

void FakePersonalizationAppUiDelegate::GetLocalImageThumbnail(
    const base::UnguessableToken& id,
    GetLocalImageThumbnailCallback callback) {
  std::move(callback).Run(std::string());
}

void FakePersonalizationAppUiDelegate::SetWallpaperObserver(
    mojo::PendingRemote<chromeos::personalization_app::mojom::WallpaperObserver>
        observer) {}

void FakePersonalizationAppUiDelegate::SelectWallpaper(
    uint64_t image_asset_id,
    SelectWallpaperCallback callback) {
  std::move(callback).Run(/*success=*/true);
}

void FakePersonalizationAppUiDelegate::SelectLocalImage(
    const base::UnguessableToken& token,
    SelectLocalImageCallback callback) {
  std::move(callback).Run(/*success=*/true);
}

void FakePersonalizationAppUiDelegate::SetCustomWallpaperLayout(
    ash::WallpaperLayout layout) {
  return;
}

void FakePersonalizationAppUiDelegate::SetDailyRefreshCollectionId(
    const std::string& collection_id) {
  return;
}

void FakePersonalizationAppUiDelegate::GetDailyRefreshCollectionId(
    GetDailyRefreshCollectionIdCallback callback) {
  std::move(callback).Run(kFakeCollectionId);
}

void FakePersonalizationAppUiDelegate::UpdateDailyRefreshWallpaper(
    UpdateDailyRefreshWallpaperCallback callback) {
  std::move(callback).Run(/*success=*/true);
}
