// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/installable/installable_icon_fetcher.h"

#include "base/check_is_test.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "build/android_buildflags.h"
#include "components/favicon/content/large_icon_service_getter.h"
#include "components/favicon/core/large_icon_service.h"
#include "components/favicon_base/favicon_types.h"
#include "components/webapps/browser/features.h"
#include "components/webapps/browser/installable/installable_evaluator.h"
#include "content/public/browser/manifest_icon_downloader.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/manifest/manifest_icon_selector.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "url/gurl.h"

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
#if !BUILDFLAG(IS_ANDROID)
    NOTREACHED_IN_MIGRATION();
#endif
    return features::kMinimumFaviconSize;
  }
}

void ProcessFaviconInBackground(
    const favicon_base::FaviconRawBitmapResult& bitmap_result,
    scoped_refptr<base::SequencedTaskRunner> ui_thread_task_runner,
    base::OnceCallback<void(const SkBitmap&)> success_callback,
    base::OnceCallback<void(InstallableStatusCode)> failed_callback) {
  SkBitmap decoded;
  if (bitmap_result.is_valid()) {
    base::AssertLongCPUWorkAllowed();
    gfx::PNGCodec::Decode(bitmap_result.bitmap_data->data(),
                          bitmap_result.bitmap_data->size(), &decoded);
  }

  int min_size = GetMinimumFaviconForPrimaryIconSizeInPx();
  if (decoded.width() < min_size || decoded.height() < min_size) {
    ui_thread_task_runner->PostTask(
        FROM_HERE, base::BindOnce(std::move(failed_callback),
                                  InstallableStatusCode::NO_ACCEPTABLE_ICON));
    return;
  }

  ui_thread_task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(success_callback), decoded));
}

#if BUILDFLAG(IS_DESKTOP_ANDROID)
// Generates a homescreen icon for `page_url` and posts a task to invoke
// `callback` on `ui_thread_task_runner.`
void GenerateHomeScreenIconInBackground(
    const GURL& page_url,
    scoped_refptr<base::SequencedTaskRunner> ui_thread_task_runner,
    base::OnceCallback<void(const GURL& url, const SkBitmap&)> callback) {
  SkBitmap bitmap =
      WebappsIconUtils::GenerateHomeScreenIconInBackground(page_url);
  ui_thread_task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), page_url, bitmap));
}
#endif  // BUILDFLAG(IS_DESKTOP_ANDROID)

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

  MaybeEndWithError(InstallableStatusCode::NO_ACCEPTABLE_ICON);
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
  favicon::LargeIconService* favicon_service =
      favicon::GetLargeIconService(web_contents_->GetBrowserContext());
  if (!favicon_service) {
    MaybeEndWithError(InstallableStatusCode::NO_ACCEPTABLE_ICON);
    return;
  }

  favicon_service->GetLargeIconRawBitmapForPageUrl(
      web_contents_->GetLastCommittedURL(),
      GetIdealPrimaryIconSizeInPx(IconPurpose::ANY),
      /*size_in_pixel_to_resize_to=*/std::nullopt,
      favicon::LargeIconService::NoBigEnoughIconBehavior::kReturnBitmap,
      base::BindOnce(&InstallableIconFetcher::OnFaviconFetched,
                     weak_ptr_factory_.GetWeakPtr()),
      &favicon_task_tracker_);
}

void InstallableIconFetcher::OnFaviconFetched(
    const favicon_base::LargeIconResult& result) {
  if (!result.bitmap.is_valid()) {
    MaybeEndWithError(InstallableStatusCode::NO_ACCEPTABLE_ICON);
    return;
  }

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&ProcessFaviconInBackground, result.bitmap,
                     base::SingleThreadTaskRunner::GetCurrentDefault(),
                     base::BindOnce(&InstallableIconFetcher::OnIconFetched,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    result.bitmap.icon_url, IconPurpose::ANY),
                     base::BindOnce(&InstallableIconFetcher::MaybeEndWithError,
                                    weak_ptr_factory_.GetWeakPtr())));
}

void InstallableIconFetcher::OnIconFetched(const GURL& icon_url,
                                           const IconPurpose purpose,
                                           const SkBitmap& bitmap) {
  page_data_->OnPrimaryIconFetched(icon_url, purpose, bitmap);
  std::move(finish_callback_).Run(InstallableStatusCode::NO_ERROR_DETECTED);
}

void InstallableIconFetcher::MaybeEndWithError(InstallableStatusCode code) {
#if BUILDFLAG(IS_DESKTOP_ANDROID)
  // Desktop android will generate an icon if none is available.
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(
          &GenerateHomeScreenIconInBackground,
          web_contents_->GetLastCommittedURL(),
          base::SingleThreadTaskRunner::GetCurrentDefault(),
          base::BindOnce(&InstallableIconFetcher::OnHomeScreenIconGenerated,
                         weak_ptr_factory_.GetWeakPtr())));
  return;
#else
  // Other platforms report an error if no icon is available.
  EndWithError(code);
#endif  // BUILDFLAG(IS_DESKTOP_ANDROID)
}

void InstallableIconFetcher::EndWithError(InstallableStatusCode code) {
  page_data_->OnPrimaryIconFetchedError(code);
  std::move(finish_callback_).Run(code);
}

#if BUILDFLAG(IS_DESKTOP_ANDROID)
void InstallableIconFetcher::OnHomeScreenIconGenerated(const GURL& page_url,
                                                       const SkBitmap& bitmap) {
  if (bitmap.drawsNothing()) {
    EndWithError(InstallableStatusCode::NO_ACCEPTABLE_ICON);
    return;
  }
  OnIconFetched(page_url, IconPurpose::ANY, bitmap);
}
#endif

}  // namespace webapps
