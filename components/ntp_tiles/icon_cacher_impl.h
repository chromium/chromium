// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_TILES_ICON_CACHER_IMPL_H_
#define COMPONENTS_NTP_TILES_ICON_CACHER_IMPL_H_

#include <memory>
#include <set>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/ntp_tiles/icon_cacher.h"
#include "components/ntp_tiles/popular_sites.h"

namespace data_decoder {
class DataDecoder;
}  // namespace data_decoder

namespace favicon {
class FaviconService;
class LargeIconService;
}  // namespace favicon

namespace favicon_base {
struct FaviconImageResult;
struct LargeIconResult;
enum class GoogleFaviconServerRequestStatus;
}  // namespace favicon_base

namespace gfx {
class Image;
}  // namespace gfx

namespace image_fetcher {
class ImageFetcher;
struct RequestMetadata;
}  // namespace image_fetcher

namespace ntp_tiles {

class IconCacherImpl : public IconCacher {
 public:
  // TODO(jkrcal): Make this eventually use only LargeIconService.
  // crbug.com/696563
  IconCacherImpl(favicon::FaviconService* favicon_service,
                 favicon::LargeIconService* large_icon_service,
                 std::unique_ptr<image_fetcher::ImageFetcher> image_fetcher,
                 std::unique_ptr<data_decoder::DataDecoder> data_decoder);

  IconCacherImpl(const IconCacherImpl&) = delete;
  IconCacherImpl& operator=(const IconCacherImpl&) = delete;

  ~IconCacherImpl() override;

  void StartFetchPopularSites(
      PopularSites::Site site,
      base::OnceClosure icon_available,
      base::OnceClosure preliminary_icon_available) override;

  // TODO(jkrcal): Rename all instances of "MostLikely" to "ChromeSuggestions".
  void StartFetchMostLikely(const GURL& page_url,
                            base::OnceClosure icon_available) override;

 private:
  using CancelableImageCallback =
      base::CancelableOnceCallback<void(const gfx::Image&)>;

  void OnGetFaviconImageForPageURLFinished(
      PopularSites::Site site,
      base::OnceClosure preliminary_icon_available,
      const favicon_base::FaviconImageResult& result);

  void OnPopularSitesFaviconDownloaded(
      PopularSites::Site site,
      std::unique_ptr<CancelableImageCallback> preliminary_callback,
      const gfx::Image& fetched_image,
      const image_fetcher::RequestMetadata& metadata);

  std::unique_ptr<CancelableImageCallback> MaybeProvideDefaultIcon(
      const PopularSites::Site& site,
      base::OnceClosure preliminary_icon_available);
  void SaveAndNotifyDefaultIconForSite(
      const PopularSites::Site& site,
      base::OnceClosure preliminary_icon_available,
      const gfx::Image& image);
  void SaveIconForSite(const PopularSites::Site& site, const gfx::Image& image);

  void OnGetLargeIconOrFallbackStyleFinished(
      const GURL& page_url,
      const favicon_base::LargeIconResult& result);

  void OnMostLikelyFaviconDownloaded(
      const GURL& request_url,
      favicon_base::GoogleFaviconServerRequestStatus status);

  bool StartRequest(const GURL& request_url, base::OnceClosure icon_available);
  void FinishRequestAndNotifyIconAvailable(const GURL& request_url,
                                           bool newly_available);

  base::CancelableTaskTracker tracker_;
  const raw_ptr<favicon::FaviconService> favicon_service_;
  const raw_ptr<favicon::LargeIconService> large_icon_service_;
  std::unique_ptr<image_fetcher::ImageFetcher> const image_fetcher_;
  std::map<GURL, std::vector<base::OnceClosure>> in_flight_requests_;
  std::unique_ptr<data_decoder::DataDecoder> data_decoder_;

  base::WeakPtrFactory<IconCacherImpl> weak_ptr_factory_{this};
};

}  // namespace ntp_tiles

#endif  // COMPONENTS_NTP_TILES_ICON_CACHER_IMPL_H_
