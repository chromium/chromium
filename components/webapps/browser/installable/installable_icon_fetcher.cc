// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/installable/installable_icon_fetcher.h"

#include "base/check_is_test.h"
#include "components/favicon/content/large_favicon_provider_getter.h"
#include "components/favicon/core/large_favicon_provider.h"
#include "components/favicon_base/favicon_types.h"
#include "components/webapps/browser/installable/installable_evaluator.h"
#include "content/public/browser/manifest_icon_downloader.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/manifest/manifest_icon_selector.h"
#include "third_party/skia/include/core/SkBitmap.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/webapps/browser/android/webapps_icon_utils.h"
#endif

namespace webapps {

namespace {

// This constant is the smallest possible adaptive launcher icon size for any
// device density.
// The ideal icon size is 83dp (see documentation for
// R.dimen.webapk_adaptive_icon_size for discussion of maskable icon size). For
// a manifest to be valid, we do NOT need an maskable icon to be 83dp for the
// device's screen density. Instead, we only need the maskable icon be larger
// than (or equal to) 83dp in the smallest screen density (that is the mdpi
// screen density). For mdpi devices, 1dp is 1px. Therefore, we have 83px here.
// Requiring the minimum icon size (in pixel) independent of the device's screen
// density is because we use mipmap-anydpi-v26 to specify adaptive launcher
// icon, and it will make the icon adaptive as long as there is one usable
// maskable icon (if that icon is of wrong size, it'll be automatically
// resized).
const int kMinimumPrimaryAdaptiveLauncherIconSizeInPx = 83;

using IconPurpose = blink::mojom::ManifestImageResource_Purpose;

int GetIdealPrimaryIconSizeInPx(IconPurpose purpose) {
#if BUILDFLAG(IS_ANDROID)
  if (purpose == IconPurpose::MASKABLE) {
    return WebappsIconUtils::GetIdealAdaptiveLauncherIconSizeInPx();
  } else {
    return WebappsIconUtils::GetIdealHomescreenIconSizeInPx();
  }
#else
  if (purpose == IconPurpose::MASKABLE) {
    return kMinimumPrimaryAdaptiveLauncherIconSizeInPx;
  } else {
    return InstallableEvaluator::GetMinimumIconSizeInPx();
  }
#endif
}

int GetMinimumPrimaryIconSizeInPx(IconPurpose purpose) {
  if (purpose == IconPurpose::MASKABLE) {
    return kMinimumPrimaryAdaptiveLauncherIconSizeInPx;
  } else {
#if BUILDFLAG(IS_ANDROID)
    return WebappsIconUtils::GetMinimumHomescreenIconSizeInPx();
#else
    return InstallableEvaluator::GetMinimumIconSizeInPx();
#endif
  }
}

// On Android, |LargeIconWorker::GetLargeIconRawBitmap| will try to find the
// largest icon that is also larger than the minimum size from database, and
// scale to the ideal size. However it doesn't work on desktop as Chrome stores
// icons scaled to 16x16 and 32x32 in the database. We need to find other way to
// fetch favicon on desktop.
int GetMinimumFaviconForPrimaryIconSizeInPx() {
  if (test::g_minimum_favicon_size_for_testing) {
    CHECK_IS_TEST();
    return test::g_minimum_favicon_size_for_testing;
  } else {
#if BUILDFLAG(IS_ANDROID)
    return WebappsIconUtils::GetMinimumHomescreenIconSizeInPx();
#else
    NOTREACHED();
    return InstallableEvaluator::GetMinimumIconSizeInPx();
#endif
  }
}

}  // namespace

namespace test {
int g_minimum_favicon_size_for_testing = 0;
}

InstallableIconFetcher::InstallableIconFetcher(
    content::WebContents* web_contents,
    InstallablePageData& data,
    const std::vector<blink::Manifest::ImageResource>& manifest_icons,
    bool prefer_maskable,
    bool fetch_favicon,
    base::OnceCallback<void(InstallableStatusCode)> finish_callback)
    : web_contents_(web_contents->GetWeakPtr()),
      page_data_(data),
      manifest_icons_(manifest_icons),
      prefer_maskable_(prefer_maskable),
      fetch_favicon_(fetch_favicon),
      finish_callback_(std::move(finish_callback)) {
  page_data_->primary_icon->fetched = true;

  downloading_icons_type_.push_back(IconPurpose::ANY);
  if (prefer_maskable_) {
    downloading_icons_type_.push_back(IconPurpose::MASKABLE);
  }

  TryFetchingNextIcon();
}

InstallableIconFetcher::~InstallableIconFetcher() = default;

void InstallableIconFetcher::TryFetchingNextIcon() {
  while (!downloading_icons_type_.empty()) {
    IconPurpose purpose = downloading_icons_type_.back();
    downloading_icons_type_.pop_back();

    GURL icon_url = blink::ManifestIconSelector::FindBestMatchingSquareIcon(
        manifest_icons_.get(), GetIdealPrimaryIconSizeInPx(purpose),
        GetMinimumPrimaryIconSizeInPx(purpose), purpose);

    if (icon_url.is_empty()) {
      continue;
    }

    bool can_download_icon = content::ManifestIconDownloader::Download(
        web_contents_.get(), icon_url, GetIdealPrimaryIconSizeInPx(purpose),
        GetMinimumPrimaryIconSizeInPx(purpose),
        InstallableEvaluator::kMaximumIconSizeInPx,
        base::BindOnce(&InstallableIconFetcher::OnManifestIconFetched,
                       weak_ptr_factory_.GetWeakPtr(), icon_url, purpose));
    if (can_download_icon) {
      // We have started to download the current icon, wait for it to complete.
      return;
    }
  }

  if (fetch_favicon_) {
    FetchFavicon();
    return;
  }

  EndWithError(NO_ACCEPTABLE_ICON);
}

void InstallableIconFetcher::OnManifestIconFetched(const GURL& icon_url,
                                                   const IconPurpose purpose,
                                                   const SkBitmap& bitmap) {
  if (bitmap.drawsNothing()) {
    TryFetchingNextIcon();
    return;
  }

  OnIconFetched(icon_url, purpose, bitmap);
}

void InstallableIconFetcher::FetchFavicon() {
  favicon::LargeFaviconProvider* favicon_provider =
      favicon::GetLargeFaviconProvider(web_contents_->GetBrowserContext());
  if (!favicon_provider) {
    EndWithError(NO_ACCEPTABLE_ICON);
    return;
  }

  favicon_provider->GetLargeIconImageOrFallbackStyleForPageUrl(
      web_contents_->GetLastCommittedURL(),
      GetMinimumFaviconForPrimaryIconSizeInPx(),
      GetIdealPrimaryIconSizeInPx(IconPurpose::ANY),
      base::BindOnce(&InstallableIconFetcher::OnFaviconFetched,
                     weak_ptr_factory_.GetWeakPtr()),
      &favicon_task_tracker_);
}

void InstallableIconFetcher::OnFaviconFetched(
    const favicon_base::LargeIconImageResult& image_result) {
  // TODO(crbug.com/1462726): add histogram to record fetched favicon size.
  if (!image_result.image.IsEmpty()) {
    OnIconFetched(image_result.icon_url, IconPurpose::ANY,
                  *image_result.image.ToSkBitmap());
    return;
  }
  EndWithError(NO_ACCEPTABLE_ICON);
}

void InstallableIconFetcher::OnIconFetched(const GURL& icon_url,
                                           const IconPurpose purpose,
                                           const SkBitmap& bitmap) {
  page_data_->primary_icon->url = icon_url;
  page_data_->primary_icon->icon = std::make_unique<SkBitmap>(bitmap);
  page_data_->primary_icon->purpose = purpose;
  page_data_->primary_icon->error = NO_ERROR_DETECTED;

  std::move(finish_callback_).Run(NO_ERROR_DETECTED);
}

void InstallableIconFetcher::EndWithError(InstallableStatusCode code) {
  page_data_->primary_icon->error = code;
  std::move(finish_callback_).Run(code);
}

}  // namespace webapps
