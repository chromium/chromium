// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/installable/installable_data_fetcher.h"

#include "base/functional/callback.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/common/constants.h"
#include "content/public/browser/manifest_icon_downloader.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"

namespace webapps {

namespace {

// Minimum dimension size in pixels for screenshots.
const int kMinimumScreenshotSizeInPx = 320;

// Maximum dimension size in pixels for screenshots.
const int kMaximumScreenshotSizeInPx = 3840;

// Maximum number of screenshots allowed, the rest will be ignored.
const int kMaximumNumOfScreenshots = 8;

}  // namespace

InstallableDataFetcher::InstallableDataFetcher(
    content::WebContents* web_contents,
    InstallablePageData& data)
    : web_contents_(web_contents->GetWeakPtr()),
      page_data_(data) {}

InstallableDataFetcher::~InstallableDataFetcher() = default;

void InstallableDataFetcher::FetchManifest(FetcherCallback finish_callback) {
  if (page_data_->manifest_fetched()) {
    // Stop and run the callback if manifest is already fetched.
    std::move(finish_callback).Run(page_data_->manifest_error());
    return;
  }

  // This uses DidFinishNavigation to abort when the primary page changes.
  // Therefore this should always be the correct page.
  web_contents()->GetPrimaryPage().GetManifest(base::BindOnce(
      &InstallableDataFetcher::OnDidGetManifest, weak_ptr_factory_.GetWeakPtr(),
      std::move(finish_callback)));
}

void InstallableDataFetcher::OnDidGetManifest(
    FetcherCallback finish_callback,
    blink::mojom::ManifestRequestResult result,
    const GURL& manifest_url,
    blink::mojom::ManifestPtr manifest) {
  InstallableStatusCode error = InstallableStatusCode::NO_ERROR_DETECTED;
  // An empty manifest signifies an error fetching, parsing, or a
  // frame/CORS/opaque origin related issue. Otherwise the manifest algorithm
  // will always populate default values for `start_url`, `id`, and `scope`.
  if (blink::IsEmptyManifest(manifest) ||
      result == blink::mojom::ManifestRequestResult::kManifestFailedToFetch ||
      result == blink::mojom::ManifestRequestResult::kManifestFailedToParse) {
    error = InstallableStatusCode::MANIFEST_PARSING_OR_NETWORK_ERROR;
  }
  page_data_->OnManifestFetched(std::move(manifest), manifest_url, error);
  std::move(finish_callback).Run(error);
}

void InstallableDataFetcher::FetchWebPageMetadata(
    FetcherCallback finish_callback) {
  if (page_data_->web_page_metadata_fetched()) {
    // Stop and run the callback if metadata is already fetched.
    std::move(finish_callback).Run(InstallableStatusCode::NO_ERROR_DETECTED);
    return;
  }

  // Send a message to the renderer to retrieve information about the page.
  mojo::AssociatedRemote<mojom::WebPageMetadataAgent> metadata_agent;
  web_contents()
      ->GetPrimaryMainFrame()
      ->GetRemoteAssociatedInterfaces()
      ->GetInterface(&metadata_agent);
  // Bind the InterfacePtr into the callback so that it's kept alive until
  // there's either a connection error or a response.
  auto* web_page_metadata_proxy = metadata_agent.get();
  web_page_metadata_proxy->GetWebPageMetadata(
      base::BindOnce(&InstallableDataFetcher::OnDidGetWebPageMetadata,
                     weak_ptr_factory_.GetWeakPtr(), std::move(metadata_agent),
                     std::move(finish_callback)));
}

void InstallableDataFetcher::OnDidGetWebPageMetadata(
    mojo::AssociatedRemote<mojom::WebPageMetadataAgent> metadata_agent,
    FetcherCallback finish_callback,
    mojom::WebPageMetadataPtr web_page_metadata) {
  page_data_->OnPageMetadataFetched(std::move(web_page_metadata));
  std::move(finish_callback).Run(InstallableStatusCode::NO_ERROR_DETECTED);
}

void InstallableDataFetcher::CheckAndFetchBestPrimaryIcon(
    FetcherCallback finish_callback,
    bool prefer_maskable,
    bool fetch_favicon) {
  if (page_data_->manifest_url().is_empty() && !fetch_favicon) {
    std::move(finish_callback).Run(InstallableStatusCode::NO_ERROR_DETECTED);
    return;
  }
  if (page_data_->primary_icon_fetched()) {
    // Stop and run the callback if an icon is already fetched.
    std::move(finish_callback).Run(page_data_->icon_error());
    return;
  }
  icon_fetcher_ = std::make_unique<InstallableIconFetcher>(
      web_contents(), *page_data_, page_data_->GetManifest().icons,
      prefer_maskable, fetch_favicon, std::move(finish_callback));
}

void InstallableDataFetcher::CheckAndFetchScreenshots(
    FetcherCallback finish_callback) {
  if (page_data_->is_screenshots_fetch_complete()) {
    // Stop and run the callback if screenshots was already fetched.
    std::move(finish_callback).Run(InstallableStatusCode::NO_ERROR_DETECTED);
    return;
  }

  screenshots_downloading_ = 0;
  screenshot_complete_ = std::move(finish_callback);

  int num_of_screenshots = 0;
  for (const auto& url : page_data_->GetManifest().screenshots) {
#if BUILDFLAG(IS_ANDROID)
    if (url->form_factor ==
        blink::mojom::ManifestScreenshot::FormFactor::kWide) {
      continue;
    }
#else
    if (url->form_factor !=
        blink::mojom::ManifestScreenshot::FormFactor::kWide) {
      continue;
    }
#endif  // BUILDFLAG(IS_ANDROID)

    if (++num_of_screenshots > kMaximumNumOfScreenshots) {
      break;
    }

    // A screenshot URL that's in the map is already taken care of.
    if (downloaded_screenshots_.count(url->image.src) > 0) {
      continue;
    }

    int ideal_size_in_px = url->image.sizes.empty()
                               ? kMinimumScreenshotSizeInPx
                               : std::max(url->image.sizes[0].width(),
                                          url->image.sizes[0].height());
    // Do not pass in a maximum icon size so that screenshots larger than
    // kMaximumScreenshotSizeInPx are not downscaled to the maximum size by
    // `ManifestIconDownloader::Download`. Screenshots with size larger than
    // kMaximumScreenshotSizeInPx get filtered out by OnScreenshotFetched.
    bool can_download = content::ManifestIconDownloader::Download(
        web_contents(), url->image.src, ideal_size_in_px,
        kMinimumScreenshotSizeInPx,
        /*maximum_icon_size_in_px=*/0,
        base::BindOnce(&InstallableDataFetcher::OnScreenshotFetched,
                       weak_ptr_factory_.GetWeakPtr(), url->image.src),
        /*square_only=*/false);
    if (can_download) {
      ++screenshots_downloading_;
    }
  }

  if (!screenshots_downloading_) {
    page_data_->OnScreenshotsDownloaded(std::vector<Screenshot>());
    std::move(screenshot_complete_)
        .Run(InstallableStatusCode::NO_ERROR_DETECTED);
  }
}

void InstallableDataFetcher::OnScreenshotFetched(GURL screenshot_url,
                                                 const SkBitmap& bitmap) {
  DCHECK_GT(screenshots_downloading_, 0);

  if (!web_contents()) {
    return;
  }

  if (!bitmap.drawsNothing()) {
    downloaded_screenshots_[screenshot_url] = bitmap;
  }

  if (--screenshots_downloading_ == 0) {
    // Now that all images have finished downloading, populate screenshots in
    // the order they are declared in the manifest.
    int num_of_screenshots = 0;
    std::vector<Screenshot> screenshots;
    for (const auto& url : page_data_->GetManifest().screenshots) {
      if (++num_of_screenshots > kMaximumNumOfScreenshots) {
        break;
      }

      auto iter = downloaded_screenshots_.find(url->image.src);
      if (iter == downloaded_screenshots_.end()) {
        continue;
      }

      auto screenshot = iter->second;
      if (screenshot.dimensions().width() > kMaximumScreenshotSizeInPx ||
          screenshot.dimensions().height() > kMaximumScreenshotSizeInPx) {
        continue;
      }

      // Screenshots must have the same aspect ratio. Cross-multiplying
      // dimensions checks portrait vs landscape mode (1:2 vs 2:1 for instance).
      if (screenshots.size() &&
          screenshot.dimensions().width() *
                  screenshots[0].image.dimensions().height() !=
              screenshot.dimensions().height() *
                  screenshots[0].image.dimensions().width()) {
        continue;
      }

      std::pair<int, int> dimensions =
          std::minmax(screenshot.width(), screenshot.height());
      if (dimensions.second > dimensions.first * kMaximumScreenshotRatio) {
        continue;
      }

      screenshots.emplace_back(std::move(screenshot), url->label);
    }

    page_data_->OnScreenshotsDownloaded(std::move(screenshots));
    downloaded_screenshots_.clear();
    std::move(screenshot_complete_)
        .Run(InstallableStatusCode::NO_ERROR_DETECTED);
  }
}

}  // namespace webapps
