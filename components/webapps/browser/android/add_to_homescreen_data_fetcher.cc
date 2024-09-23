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
#include "build/android_buildflags.h"
#include "components/dom_distiller/core/url_utils.h"
#include "components/favicon/content/large_icon_service_getter.h"
#include "components/favicon/core/large_icon_service.h"
#include "components/favicon_base/favicon_types.h"
#include "components/webapps/browser/android/webapps_icon_utils.h"
#include "components/webapps/browser/android/webapps_utils.h"
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
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-shared.h"
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
  params.fetch_favicon = true;
  return params;
}

InstallableParams ParamsToPerformInstallableCheck() {
  InstallableParams params;
  params.check_eligibility = true;
    params.installable_criteria = InstallableCriteria::kNoManifestAtRootScope;
  return params;
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
  PrepareToAddShortcut();
}

void AddToHomescreenDataFetcher::OnDidGetInstallableData(
    const InstallableData& data) {
  if (!web_contents_)
    return;

  RecordMobileCapableUserActions(
      data.web_page_metadata->mobile_capable,
      /*has_manifest=*/!data.manifest_url->is_empty());

  shortcut_info_.UpdateFromWebPageMetadata(*data.web_page_metadata);
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
    PrepareToAddShortcut();
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

  installable_status_code_ = data.GetFirstError();

#if BUILDFLAG(IS_DESKTOP_ANDROID)
  constexpr bool is_desktop_android = true;
#else
  constexpr bool is_desktop_android = false;
#endif

  // Desktop Android is allowed to install incompatible sites as apps.
  // Regular Android installs them as shortcuts.
  // TODO(b/362975239): Fix the compatibility check on desktop Android.
  if (!webapk_compatible && !is_desktop_android) {
    PrepareToAddShortcut();
    return;
  }

  shortcut_info_.UpdateDisplayMode(webapk_compatible);

  AddToHomescreenParams::AppType app_type =
      data.manifest_url->is_empty() ? AddToHomescreenParams::AppType::WEBAPK_DIY
                                    : AddToHomescreenParams::AppType::WEBAPK;

  observer_->OnUserTitleAvailable(
      webapk_compatible ? shortcut_info_.name : shortcut_info_.user_title,
      shortcut_info_.url, app_type);

  // WebAPKs should always use the raw icon for the launcher whether or not
  // that icon is maskable.
  primary_icon_ = raw_primary_icon_;
  observer_->OnDataAvailable(shortcut_info_, primary_icon_, app_type,
                             installable_status_code_);
}

void AddToHomescreenDataFetcher::PrepareToAddShortcut() {
  observer_->OnUserTitleAvailable(shortcut_info_.user_title, shortcut_info_.url,
                                  AddToHomescreenParams::AppType::SHORTCUT);
  StopTimer();
  CreateIconForView(raw_primary_icon_);
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
      base::BindOnce(&WebappsIconUtils::FinalizeLauncherIconInBackground,
                     base_icon, shortcut_info_.url,
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
                             AddToHomescreenParams::AppType::SHORTCUT,
                             installable_status_code_);
}

}  // namespace webapps
