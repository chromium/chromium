// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_CUSTOMIZE_CHROME_WALLPAPER_SEARCH_WALLPAPER_SEARCH_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_CUSTOMIZE_CHROME_WALLPAPER_SEARCH_WALLPAPER_SEARCH_HANDLER_H_

#include <tuple>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/token.h"
#include "chrome/browser/search/background/wallpaper_search/wallpaper_search_data.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/wallpaper_search/wallpaper_search.mojom.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/prefs/pref_change_registrar.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkBitmap.h"

class Profile;
class WallpaperSearchBackgroundManager;

namespace data_decoder {
class DataDecoder;
}  // namespace data_decoder

namespace gfx {
class Image;
}

namespace image_fetcher {
class ImageDecoder;
using ImageDecodedCallback = base::OnceCallback<void(const gfx::Image&)>;
}  // namespace image_fetcher

class WallpaperSearchHandler
    : public side_panel::customize_chrome::mojom::WallpaperSearchHandler {
 public:
  WallpaperSearchHandler(
      mojo::PendingReceiver<
          side_panel::customize_chrome::mojom::WallpaperSearchHandler>
          pending_handler,
      mojo::PendingRemote<
          side_panel::customize_chrome::mojom::WallpaperSearchClient>
          pending_client,
      Profile* profile,
      image_fetcher::ImageDecoder* image_decoder,
      WallpaperSearchBackgroundManager* wallpaper_search_background_manager,
      int64_t session_id);

  WallpaperSearchHandler(const WallpaperSearchHandler&) = delete;
  WallpaperSearchHandler& operator=(const WallpaperSearchHandler&) = delete;

  ~WallpaperSearchHandler() override;

  // side_panel::customize_chrome::mojom::WallpaperSearchHandler:
  void GetDescriptors(GetDescriptorsCallback callback) override;
  void GetInspirations(GetInspirationsCallback callback) override;
  void GetWallpaperSearchResults(
      side_panel::customize_chrome::mojom::ResultDescriptorsPtr
          result_descriptors,
      GetWallpaperSearchResultsCallback callback) override;
  void SetResultRenderTime(const std::vector<base::Token>& result_ids,
                           double time) override;
  void SetBackgroundToHistoryImage(
      const base::Token& result_id,
      side_panel::customize_chrome::mojom::ResultDescriptorsPtr descriptors)
      override;
  void SetBackgroundToWallpaperSearchResult(
      const base::Token& result_id,
      double time,
      side_panel::customize_chrome::mojom::ResultDescriptorsPtr descriptors)
      override;
  void UpdateHistory() override;
  void SetUserFeedback(side_panel::customize_chrome::mojom::UserFeedback
                           selected_option) override;
  void OpenHelpArticle() override;
#if BUILDFLAG(IS_CHROMEOS)
  void SkipShowFeedbackPageForTesting(bool should_skip_check) {
    skip_show_feedback_page_for_testing_ = should_skip_check;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

 private:
  void ShowFeedbackPage();
  void DecodeHistoryImage(image_fetcher::ImageDecodedCallback callback,
                          std::string image);
  void OnDescriptorsRetrieved(GetDescriptorsCallback callback,
                              std::unique_ptr<std::string> response_body);
  void OnDescriptorsJsonParsed(GetDescriptorsCallback callback,
                               data_decoder::DataDecoder::ValueOrError result);
  void OnHistoryDecoded(std::vector<HistoryEntry> history,
                        std::vector<std::pair<SkBitmap, base::Token>> results);
  void OnInspirationsRetrieved(GetInspirationsCallback callback,
                               std::unique_ptr<std::string> response_body);
  void OnInspirationsJsonParsed(GetInspirationsCallback callback,
                                data_decoder::DataDecoder::ValueOrError result);
  void OnWallpaperSearchResultsRetrieved(
      GetWallpaperSearchResultsCallback callback,
      base::ElapsedTimer request_timer,
      optimization_guide::OptimizationGuideModelExecutionResult result,
      std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry);
  void OnWallpaperSearchResultsDecoded(
      GetWallpaperSearchResultsCallback callback,
      base::ElapsedTimer processing_timer,
      std::vector<
          std::pair<optimization_guide::proto::WallpaperSearchImageQuality*,
                    SkBitmap>> bitmaps);
  void SelectHistoryImage(
      const base::Token& id,
      base::ElapsedTimer timer,
      side_panel::customize_chrome::mojom::ResultDescriptorsPtr descriptors,
      const gfx::Image& image);

  raw_ptr<Profile> profile_;
  PrefChangeRegistrar pref_change_registrar_;
  std::unique_ptr<network::SimpleURLLoader> descriptors_simple_url_loader_;
  std::unique_ptr<data_decoder::DataDecoder> data_decoder_;
  const raw_ref<image_fetcher::ImageDecoder> image_decoder_;
  std::unique_ptr<network::SimpleURLLoader> inspirations_simple_url_loader_;
  const raw_ref<WallpaperSearchBackgroundManager>
      wallpaper_search_background_manager_;
  // We keep all log entries alive until the session closes because whether and
  // which image was selected will only be known then.
  std::vector<
      std::pair<std::unique_ptr<optimization_guide::ModelQualityLogEntry>,
                std::optional<base::Time>>>
      log_entries_;
  // Theme to be sent to the background manager to be saved to history on
  // destruction of this handler.
  std::unique_ptr<HistoryEntry> history_entry_;
  // `wallpaper_search_results_` points to entries in `log_entries_`. Therefore,
  // `wallpaper_search_results_` is defined below so that the pointers get
  // destructed before the pointed to objects in `log_entries_`.
  base::flat_map<
      base::Token,
      std::tuple<optimization_guide::proto::WallpaperSearchImageQuality*,
                 absl::optional<base::Time>,
                 SkBitmap>>
      wallpaper_search_results_;
  const int64_t session_id_;
#if BUILDFLAG(IS_CHROMEOS)
  bool skip_show_feedback_page_for_testing_ = false;
#endif  // BUILDFLAG(IS_CHROMEOS)

  mojo::Remote<side_panel::customize_chrome::mojom::WallpaperSearchClient>
      client_;
  mojo::Receiver<side_panel::customize_chrome::mojom::WallpaperSearchHandler>
      receiver_;

  base::WeakPtrFactory<WallpaperSearchHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_CUSTOMIZE_CHROME_WALLPAPER_SEARCH_WALLPAPER_SEARCH_HANDLER_H_
