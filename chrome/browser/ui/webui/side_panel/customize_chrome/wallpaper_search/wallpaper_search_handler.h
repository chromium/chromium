// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_CUSTOMIZE_CHROME_WALLPAPER_SEARCH_WALLPAPER_SEARCH_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_CUSTOMIZE_CHROME_WALLPAPER_SEARCH_WALLPAPER_SEARCH_HANDLER_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/token.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/wallpaper_search/wallpaper_search.mojom.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/skia/include/core/SkBitmap.h"

class Profile;

namespace data_decoder {
class DataDecoder;
}  // namespace data_decoder

namespace image_fetcher {
class ImageDecoder;
}  // namespace image_fetcher

class WallpaperSearchHandler
    : public side_panel::customize_chrome::mojom::WallpaperSearchHandler {
 public:
  WallpaperSearchHandler(
      mojo::PendingReceiver<
          side_panel::customize_chrome::mojom::WallpaperSearchHandler>
          pending_handler,
      Profile* profile,
      image_fetcher::ImageDecoder* image_decoder);

  WallpaperSearchHandler(const WallpaperSearchHandler&) = delete;
  WallpaperSearchHandler& operator=(const WallpaperSearchHandler&) = delete;

  ~WallpaperSearchHandler() override;

  // side_panel::customize_chrome::mojom::WallpaperSearchHandler:
  void GetDescriptors(GetDescriptorsCallback callback) override;
  void GetWallpaperSearchResults(
      const std::string& descriptor_a,
      const absl::optional<std::string>& descriptor_b,
      const absl::optional<std::string>& descriptor_c,
      side_panel::customize_chrome::mojom::DescriptorDValuePtr
          descriptor_d_value,
      GetWallpaperSearchResultsCallback callback) override;
  void SetBackgroundToWallpaperSearchResult(
      const base::Token& result_id) override;

 private:
  void OnDescriptorsRetrieved(GetDescriptorsCallback callback,
                              std::unique_ptr<std::string> response_body);
  void OnDescriptorsJsonParsed(GetDescriptorsCallback callback,
                               data_decoder::DataDecoder::ValueOrError result);
  void OnWallpaperSearchResultsRetrieved(
      GetWallpaperSearchResultsCallback callback,
      optimization_guide::OptimizationGuideModelExecutionResult result);
  void OnWallpaperSearchResultsDecoded(
      GetWallpaperSearchResultsCallback callback,
      std::vector<SkBitmap> bitmaps);

  raw_ptr<Profile> profile_;
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;
  std::unique_ptr<data_decoder::DataDecoder> data_decoder_;
  const raw_ref<image_fetcher::ImageDecoder> image_decoder_;
  base::flat_map<base::Token, SkBitmap> wallpaper_search_results_;

  mojo::Receiver<side_panel::customize_chrome::mojom::WallpaperSearchHandler>
      receiver_;

  base::WeakPtrFactory<WallpaperSearchHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_CUSTOMIZE_CHROME_WALLPAPER_SEARCH_WALLPAPER_SEARCH_HANDLER_H_
