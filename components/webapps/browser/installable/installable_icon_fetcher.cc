// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/installable/installable_icon_fetcher.h"

#include <algorithm>
#include <vector>

#include "base/check_is_test.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_util.h"
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
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "url/gurl.h"
#include "url/url_constants.h"

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
  if (test::g_ideal_favicon_size_for_testing) {
    CHECK_IS_TEST();
    return test::g_ideal_favicon_size_for_testing;
  }
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
// scale to the ideal size. If the database does not have a large enough icon
// (such as on Desktop Android where only 16dp favicons are cached), we fall
// back to downloading favicon candidates from the DOM on demand.
int GetMinimumFaviconForPrimaryIconSizeInPx() {
  if (test::g_minimum_favicon_size_for_testing) {
    CHECK_IS_TEST();
    return test::g_minimum_favicon_size_for_testing;
  } else {
#if BUILDFLAG(IS_ANDROID)
    return features::kMinimumFaviconSize;
#else
    return InstallableEvaluator::GetMinimumIconSizeInPx();
#endif
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
    decoded = gfx::PNGCodec::Decode(*bitmap_result.bitmap_data);
  }

  int min_size = GetMinimumFaviconForPrimaryIconSizeInPx();
  if (decoded.isNull() || decoded.width() < min_size ||
      decoded.height() < min_size) {
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

bool IsIconSvg(const GURL& icon_url) {
  if (base::EndsWith(icon_url.ExtractFileName(), ".svg",
                     base::CompareCase::INSENSITIVE_ASCII)) {
    return true;
  }
  if (icon_url.SchemeIs(url::kDataScheme) &&
      icon_url.GetContentPiece().starts_with("image/svg+xml")) {
    return true;
  }
  return false;
}

bool IsCandidateBigEnough(const blink::mojom::FaviconURL& favicon_url,
                          int ideal_size) {
  if (favicon_url.icon_type == blink::mojom::FaviconIconType::kInvalid ||
      !favicon_url.icon_url.is_valid() || favicon_url.is_default_icon) {
    return false;
  }

  if (IsIconSvg(favicon_url.icon_url)) {
    return true;
  }

  for (const auto& size : favicon_url.icon_sizes) {
    if (size.IsEmpty()) {
      return true;
    }
    if (size.width() >= ideal_size && size.height() >= ideal_size) {
      return true;
    }
  }

  if (favicon_url.icon_sizes.empty() &&
      (favicon_url.icon_type == blink::mojom::FaviconIconType::kTouchIcon ||
       favicon_url.icon_type ==
           blink::mojom::FaviconIconType::kTouchPrecomposedIcon)) {
    constexpr int kEstimatedTouchIconSize = 180;
    return kEstimatedTouchIconSize >= ideal_size;
  }

  return false;
}

}  // namespace

namespace test {
int g_minimum_favicon_size_for_testing = 0;
int g_ideal_favicon_size_for_testing = 0;
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
  TryFetchingNextIcon();
}

InstallableIconFetcher::~InstallableIconFetcher() = default;

void InstallableIconFetcher::TryFetchingNextIcon() {
  blink::ManifestIconSelectorParams params;
  params.purpose = prefer_maskable_ ? IconPurpose::MASKABLE : IconPurpose::ANY;

  params.ideal_icon_size_in_px = GetIdealPrimaryIconSizeInPx(params.purpose);
  params.minimum_icon_size_in_px =
      GetMinimumPrimaryIconSizeInPx(params.purpose);

  auto result = blink::ManifestIconSelector::FindBestMatchingIcon(
      manifest_icons_.get(), params);
  if (!result && prefer_maskable_) {
    prefer_maskable_ = false;
    TryFetchingNextIcon();
    return;
  }

  if (result) {
    bool can_download_icon = content::ManifestIconDownloader::Download(
        web_contents_.get(), result->icon_url,
        GetIdealPrimaryIconSizeInPx(result->icon_purpose),
        GetMinimumPrimaryIconSizeInPx(result->icon_purpose),
        InstallableEvaluator::kMaximumIconSizeInPx,
        base::BindOnce(&InstallableIconFetcher::OnManifestIconFetched,
                       weak_ptr_factory_.GetWeakPtr(), result->icon_url,
                       result->icon_purpose));
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
    if (prefer_maskable_) {
      // We preferred a maskable icon, but the one we got was invalid (e.g.,
      // corrupted). Try again without preferring masking.
      prefer_maskable_ = false;
      TryFetchingNextIcon();
    } else {
      MaybeEndWithError(InstallableStatusCode::NO_ACCEPTABLE_ICON);
    }
    return;
  }

  OnIconFetched(icon_url, purpose, bitmap);
}

void InstallableIconFetcher::FetchFavicon() {
  if (!web_contents_) {
    MaybeEndWithError(InstallableStatusCode::NO_ACCEPTABLE_ICON);
    return;
  }

  favicon::LargeIconService* favicon_service =
      favicon::GetLargeIconService(web_contents_->GetBrowserContext());
  if (!favicon_service) {
    FetchFaviconFromCandidates();
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
    FetchFaviconFromCandidates();
    return;
  }

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(
          &ProcessFaviconInBackground, result.bitmap,
          base::SingleThreadTaskRunner::GetCurrentDefault(),
          base::BindOnce(&InstallableIconFetcher::OnIconFetched,
                         weak_ptr_factory_.GetWeakPtr(), result.bitmap.icon_url,
                         IconPurpose::ANY),
          base::BindOnce(&InstallableIconFetcher::OnFaviconProcessingFailed,
                         weak_ptr_factory_.GetWeakPtr())));
}

void InstallableIconFetcher::OnFaviconProcessingFailed(
    InstallableStatusCode code) {
  FetchFaviconFromCandidates();
}

void InstallableIconFetcher::FetchFaviconFromCandidates() {
  if (!web_contents_) {
    MaybeEndWithError(InstallableStatusCode::NO_ACCEPTABLE_ICON);
    return;
  }

  const int min_size = GetMinimumFaviconForPrimaryIconSizeInPx();
  const int ideal_size =
      std::max(GetIdealPrimaryIconSizeInPx(IconPurpose::ANY), min_size);
  const int max_size =
      std::max(InstallableEvaluator::kMaximumIconSizeInPx, ideal_size);

  for (const auto& favicon_url : web_contents_->GetFaviconURLs()) {
    if (!IsCandidateBigEnough(*favicon_url, ideal_size)) {
      continue;
    }

    bool can_download_icon = content::ManifestIconDownloader::Download(
        web_contents_.get(), favicon_url->icon_url, ideal_size, min_size,
        max_size,
        base::BindOnce(&InstallableIconFetcher::OnFaviconCandidateDownloaded,
                       weak_ptr_factory_.GetWeakPtr(), favicon_url->icon_url),
        /*square_only=*/true,
        /*initiator_frame_routing_id=*/content::GlobalRenderFrameHostId(),
        /*suppress_warnings=*/true);
    if (can_download_icon) {
      return;
    }
  }

  // No big enough candidate was found.
  MaybeEndWithError(InstallableStatusCode::NO_ACCEPTABLE_ICON);
}

void InstallableIconFetcher::OnFaviconCandidateDownloaded(
    const GURL& icon_url,
    const SkBitmap& bitmap) {
  if (bitmap.drawsNothing()) {
    MaybeEndWithError(InstallableStatusCode::NO_ACCEPTABLE_ICON);
    return;
  }

  OnIconFetched(icon_url, IconPurpose::ANY, bitmap);
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
  if (!web_contents_) {
    EndWithError(code);
    return;
  }
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
