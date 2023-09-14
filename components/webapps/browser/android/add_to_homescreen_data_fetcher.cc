// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/android/add_to_homescreen_data_fetcher.h"

#include <utility>
#include <vector>

#include "base/android/build_info.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "components/dom_distiller/core/url_utils.h"
#include "components/favicon/content/large_favicon_provider_getter.h"
#include "components/favicon/core/large_favicon_provider.h"
#include "components/favicon_base/favicon_types.h"
#include "components/webapps/browser/android/webapps_icon_utils.h"
#include "components/webapps/browser/android/webapps_utils.h"
#include "components/webapps/browser/features.h"
#include "components/webapps/browser/installable/installable_manager.h"
#include "components/webapps/common/constants.h"
#include "components/webapps/common/web_page_metadata.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/common/manifest/manifest_icon_selector.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/favicon_size.h"
#include "url/gurl.h"

namespace webapps {

namespace {

// Looks up the original, online, visible URL of |web_contents|. The current
// visible URL may be a distilled article which is not appropriate for a home
// screen shortcut.
GURL GetShortcutUrl(content::WebContents* web_contents) {
  return dom_distiller::url_utils::GetOriginalUrlFromDistillerUrl(
      web_contents->GetVisibleURL());
}

InstallableParams ParamsToFetchInstallableData() {
  // Fetch manifest and metadata.
  InstallableParams params;
  params.fetch_metadata = true;
  return params;
}

InstallableParams ParamsToFetchPrimaryIcon() {
  InstallableParams params;
  params.valid_primary_icon = true;
  params.prefer_maskable_icon =
      WebappsIconUtils::DoesAndroidSupportMaskableIcons();
  params.fetch_favicon =
      base::FeatureList::IsEnabled(features::kUniversalInstallIcon);
  return params;
}

InstallableParams ParamsToPerformInstallableCheck() {
  InstallableParams params;
  params.check_eligibility = true;
  if (base::FeatureList::IsEnabled(features::kUniversalInstallManifest)) {
    params.installable_criteria =
        InstallableCriteria::kImplicitManifestFieldsHTML;
  } else {
    params.installable_criteria = InstallableCriteria::kValidManifestWithIcons;
  }
  return params;
}

// Creates a launcher icon from |icon|. |start_url| is used to generate the icon
// if |icon| is empty or is not large enough. When complete, posts |callback| on
// |ui_thread_task_runner| binding:
// - the generated icon
// - whether |icon| was used in generating the launcher icon
void CreateLauncherIconInBackground(
    const GURL& start_url,
    const SkBitmap& icon,
    scoped_refptr<base::SequencedTaskRunner> ui_thread_task_runner,
    base::OnceCallback<void(const SkBitmap&, bool)> callback) {
  bool is_generated = false;
  SkBitmap primary_icon = WebappsIconUtils::FinalizeLauncherIconInBackground(
      icon, start_url, &is_generated);
  ui_thread_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), primary_icon, is_generated));
}

// Creates a launcher icon from |bitmap_result|. |start_url| is used to
// generate the icon if there is no bitmap in |bitmap_result| or the bitmap is
// not large enough.
void CreateLauncherIconFromFaviconInBackground(
    const GURL& start_url,
    const favicon_base::FaviconRawBitmapResult& bitmap_result,
    scoped_refptr<base::SequencedTaskRunner> ui_thread_task_runner,
    base::OnceCallback<void(const SkBitmap&, bool)> callback) {
  SkBitmap decoded;
  if (bitmap_result.is_valid()) {
    base::AssertLongCPUWorkAllowed();
    gfx::PNGCodec::Decode(bitmap_result.bitmap_data->front(),
                          bitmap_result.bitmap_data->size(), &decoded);
  }
  CreateLauncherIconInBackground(start_url, decoded, ui_thread_task_runner,
                                 std::move(callback));
}

void RecordAddToHomescreenDialogDuration(base::TimeDelta duration) {
  UMA_HISTOGRAM_TIMES("Webapp.AddToHomescreenDialog.Timeout", duration);
}

void RecordMobileCapableUserActions(mojom::WebPageMobileCapable mobile_capable,
                                    bool has_manifest) {
  if (has_manifest) {
    base::RecordAction(base::UserMetricsAction("webapps.AddShortcut.Manifest"));
    return;
  }

  // Record the use of web-app-capable meta flag.
  switch (mobile_capable) {
    case mojom::WebPageMobileCapable::ENABLED:
      base::RecordAction(
          base::UserMetricsAction("webapps.AddShortcut.AppShortcut"));
      break;
    case mojom::WebPageMobileCapable::ENABLED_APPLE:
      base::RecordAction(
          base::UserMetricsAction("webapps.AddShortcut.AppShortcutApple"));
      break;
    case mojom::WebPageMobileCapable::UNSPECIFIED:
      base::RecordAction(
          base::UserMetricsAction("webapps.AddShortcut.Bookmark"));
      break;
  }
}

}  // namespace

AddToHomescreenDataFetcher::AddToHomescreenDataFetcher(
    content::WebContents* web_contents,
    int data_timeout_ms,
    Observer* observer)
    : web_contents_(web_contents->GetWeakPtr()),
      installable_manager_(InstallableManager::FromWebContents(web_contents)),
      observer_(observer),
      shortcut_info_(GetShortcutUrl(web_contents)),
      data_timeout_ms_(base::Milliseconds(data_timeout_ms)) {
  DCHECK(shortcut_info_.url.is_valid());
  shortcut_info_.user_title = web_contents_->GetTitle();

  FetchInstallableData();
}

AddToHomescreenDataFetcher::~AddToHomescreenDataFetcher() = default;

void AddToHomescreenDataFetcher::FetchInstallableData() {
  // Kick off a timeout for downloading web app data. If we haven't
  // finished within the timeout, fall back to using any fetched icon, or at
  // worst, a dynamically-generated launcher icon.
  data_timeout_timer_.Start(
      FROM_HERE, data_timeout_ms_,
      base::BindOnce(&AddToHomescreenDataFetcher::OnDataTimedout,
                     weak_ptr_factory_.GetWeakPtr()));
  start_time_ = base::TimeTicks::Now();

  installable_manager_->GetData(
      ParamsToFetchInstallableData(),
      base::BindOnce(&AddToHomescreenDataFetcher::OnDidGetInstallableData,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AddToHomescreenDataFetcher::StopTimer() {
  if (data_timeout_timer_.IsRunning()) {
    data_timeout_timer_.Stop();
    RecordAddToHomescreenDialogDuration(base::TimeTicks::Now() - start_time_);
  }
}

void AddToHomescreenDataFetcher::OnDataTimedout() {
  RecordAddToHomescreenDialogDuration(data_timeout_ms_);
  weak_ptr_factory_.InvalidateWeakPtrs();

  if (!web_contents_)
    return;

  installable_status_code_ = InstallableStatusCode::DATA_TIMED_OUT;
  PrepareToAddShortcut(false /* fetch_favicon */);
}

void AddToHomescreenDataFetcher::OnDidGetInstallableData(
    const InstallableData& data) {
  if (!web_contents_)
    return;

  shortcut_info_.UpdateFromWebPageMetadata(*data.web_page_metadata);

  RecordMobileCapableUserActions(data.web_page_metadata->mobile_capable,
                                 !blink::IsEmptyManifest(*data.manifest));

  if (blink::IsEmptyManifest(*data.manifest)) {
    installable_status_code_ = data.GetFirstError();
    PrepareToAddShortcut(true /* fetch_favicon */);
    return;
  }

  shortcut_info_.UpdateFromManifest(*data.manifest);
  shortcut_info_.manifest_url = (*data.manifest_url);
  // Save the splash screen URL for the later download.
  shortcut_info_.UpdateBestSplashIcon(*data.manifest);

  installable_manager_->GetData(
      ParamsToFetchPrimaryIcon(),
      base::BindOnce(&AddToHomescreenDataFetcher::OnDidGetPrimaryIcon,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AddToHomescreenDataFetcher::OnDidGetPrimaryIcon(
    const InstallableData& data) {
  if (!web_contents_) {
    return;
  }

  if (!data.primary_icon) {
    installable_status_code_ = data.GetFirstError();
    PrepareToAddShortcut(
        !base::FeatureList::IsEnabled(features::kUniversalInstallIcon));
    return;
  }

  raw_primary_icon_ = *data.primary_icon;
  shortcut_info_.best_primary_icon_url = (*data.primary_icon_url);
  shortcut_info_.is_primary_icon_maskable = data.has_maskable_primary_icon;

  installable_manager_->GetData(
      ParamsToPerformInstallableCheck(),
      base::BindOnce(&AddToHomescreenDataFetcher::OnDidPerformInstallableCheck,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AddToHomescreenDataFetcher::OnDidPerformInstallableCheck(
    const InstallableData& data) {
  StopTimer();

  if (!web_contents_)
    return;

  bool webapk_compatible =
      (data.errors.empty() && data.installable_check_passed &&
       WebappsUtils::AreWebManifestUrlsWebApkCompatible(*data.manifest));

  observer_->OnUserTitleAvailable(
      webapk_compatible ? shortcut_info_.name : shortcut_info_.user_title,
      shortcut_info_.url, webapk_compatible);

  shortcut_info_.UpdateDisplayMode(webapk_compatible);

  if (webapk_compatible) {
    // WebAPKs should always use the raw icon for the launcher whether or not
    // that icon is maskable.
    primary_icon_ = raw_primary_icon_;
    shortcut_info_.UpdateSource(ShortcutInfo::SOURCE_ADD_TO_HOMESCREEN_PWA);
    // We can skip creating an icon for the view because the raw icon is
    // sufficient when WebAPK-compatible.
    OnIconCreated(raw_primary_icon_,
                  /*is_icon_generated=*/false);
    return;
  }

  installable_status_code_ = data.GetFirstError();

  CreateIconForView(raw_primary_icon_);
}

void AddToHomescreenDataFetcher::PrepareToAddShortcut(bool fetch_favicon) {
  observer_->OnUserTitleAvailable(shortcut_info_.user_title, shortcut_info_.url,
                                  /*is_webapk_compatible=*/false);
  StopTimer();
  if (fetch_favicon) {
    FetchFavicon();
    return;
  }
  CreateIconForView(raw_primary_icon_);
}

void AddToHomescreenDataFetcher::FetchFavicon() {
  if (!web_contents_)
    return;

  // Using favicon if its size is not smaller than platform required size,
  // otherwise using the largest icon among all available icons.
  int threshold_to_get_any_largest_icon =
      WebappsIconUtils::GetIdealHomescreenIconSizeInPx() - 1;
  favicon::GetLargeFaviconProvider(web_contents_->GetBrowserContext())
      ->GetLargeIconRawBitmapForPageUrl(
          shortcut_info_.url, threshold_to_get_any_largest_icon,
          base::BindOnce(&AddToHomescreenDataFetcher::OnFaviconFetched,
                         weak_ptr_factory_.GetWeakPtr()),
          &favicon_task_tracker_);
}

void AddToHomescreenDataFetcher::OnFaviconFetched(
    const favicon_base::FaviconRawBitmapResult& bitmap_result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!web_contents_) {
    return;
  }

  shortcut_info_.best_primary_icon_url = bitmap_result.icon_url;

  // The user is waiting for the icon to be processed before they can
  // proceed with add to homescreen. But if we shut down, there's no point
  // starting the image processing. Use USER_VISIBLE with MayBlock and
  // SKIP_ON_SHUTDOWN.
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&CreateLauncherIconFromFaviconInBackground,
                     shortcut_info_.url, bitmap_result,
                     base::SingleThreadTaskRunner::GetCurrentDefault(),
                     base::BindOnce(&AddToHomescreenDataFetcher::OnIconCreated,
                                    weak_ptr_factory_.GetWeakPtr())));
}

void AddToHomescreenDataFetcher::CreateIconForView(const SkBitmap& base_icon) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // The user is waiting for the icon to be processed before they can proceed
  // with add to homescreen. But if we shut down, there's no point starting the
  // image processing. Use USER_VISIBLE with MayBlock and SKIP_ON_SHUTDOWN.
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&CreateLauncherIconInBackground, shortcut_info_.url,
                     base_icon,
                     base::SingleThreadTaskRunner::GetCurrentDefault(),
                     base::BindOnce(&AddToHomescreenDataFetcher::OnIconCreated,
                                    weak_ptr_factory_.GetWeakPtr())));
}

void AddToHomescreenDataFetcher::OnIconCreated(const SkBitmap& icon_for_view,
                                               bool is_icon_generated) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!web_contents_)
    return;

  primary_icon_ = icon_for_view;
  if (is_icon_generated) {
    shortcut_info_.best_primary_icon_url = GURL();
    shortcut_info_.is_primary_icon_maskable = false;
  }

  observer_->OnDataAvailable(shortcut_info_, icon_for_view,
                             installable_status_code_);
}

}  // namespace webapps
