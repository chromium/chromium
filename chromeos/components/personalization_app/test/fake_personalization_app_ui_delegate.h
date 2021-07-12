// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PERSONALIZATION_APP_TEST_FAKE_PERSONALIZATION_APP_UI_DELEGATE_H_
#define CHROMEOS_COMPONENTS_PERSONALIZATION_APP_TEST_FAKE_PERSONALIZATION_APP_UI_DELEGATE_H_

#include "chromeos/components/personalization_app/personalization_app_ui_delegate.h"

#include <stdint.h>

#include "base/unguessable_token.h"
#include "chromeos/components/personalization_app/mojom/personalization_app.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content {
class WebUI;
}  // namespace content

class FakePersonalizationAppUiDelegate : public PersonalizationAppUiDelegate {
 public:
  explicit FakePersonalizationAppUiDelegate(content::WebUI* web_ui);

  FakePersonalizationAppUiDelegate(const FakePersonalizationAppUiDelegate&) =
      delete;
  FakePersonalizationAppUiDelegate& operator=(
      const FakePersonalizationAppUiDelegate&) = delete;

  ~FakePersonalizationAppUiDelegate() override;

  // PersonalizationAppUIDelegate:
  void BindInterface(mojo::PendingReceiver<
                     chromeos::personalization_app::mojom::WallpaperProvider>
                         receiver) override;

  void FetchCollections(FetchCollectionsCallback callback) override;

  void FetchImagesForCollection(
      const std::string& collection_id,
      FetchImagesForCollectionCallback callback) override;

  void GetLocalImages(GetLocalImagesCallback callback) override;

  void GetLocalImageThumbnail(const base::UnguessableToken& id,
                              GetLocalImageThumbnailCallback callback) override;

  void GetCurrentWallpaper(GetCurrentWallpaperCallback callback) override;

  void SelectWallpaper(uint64_t image_asset_id,
                       SelectWallpaperCallback callback) override;

  void SelectLocalImage(const base::UnguessableToken& token,
                        SelectLocalImageCallback callback) override;

  void SetCustomWallpaperLayout(ash::WallpaperLayout layout) override;

 private:
  mojo::Receiver<chromeos::personalization_app::mojom::WallpaperProvider>
      wallpaper_receiver_{this};
};

#endif  // CHROMEOS_COMPONENTS_PERSONALIZATION_APP_TEST_FAKE_PERSONALIZATION_APP_UI_DELEGATE_H_
