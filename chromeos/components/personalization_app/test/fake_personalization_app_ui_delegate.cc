// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/personalization_app/test/fake_personalization_app_ui_delegate.h"

#include <stdint.h>

#include "base/check_op.h"
#include "chromeos/components/personalization_app/mojom/personalization_app.mojom-forward.h"
#include "chromeos/components/personalization_app/mojom/personalization_app.mojom.h"
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
  std::vector<chromeos::personalization_app::mojom::WallpaperCollectionPtr>
      collections;
  collections.push_back(
      chromeos::personalization_app::mojom::WallpaperCollection::New(
          kFakeCollectionId, "Test Collection", absl::optional<GURL>()));
  std::move(callback).Run(std::move(collections));
}

void FakePersonalizationAppUiDelegate::FetchImagesForCollection(
    const std::string& collection_id,
    FetchImagesForCollectionCallback callback) {
  DCHECK_EQ(collection_id, kFakeCollectionId);
  std::vector<chromeos::personalization_app::mojom::WallpaperImagePtr> images;
  images.push_back(
      chromeos::personalization_app::mojom::WallpaperImage::New(GURL(), 0));
  std::move(callback).Run(std::move(images));
}

void FakePersonalizationAppUiDelegate::SelectWallpaper(
    uint64_t image_asset_id,
    SelectWallpaperCallback callback) {
  std::move(callback).Run(/*success=*/true);
}
