// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/android/add_to_homescreen_data_fetcher.h"

#include <algorithm>
#include <initializer_list>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "components/dom_distiller/core/url_utils.h"
#include "components/favicon/content/large_icon_service_getter.h"
#include "components/favicon/core/large_icon_service.h"
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
      web_contents->GetLastCommittedURL());
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
  params.prefer_maskable_icon = true;
  params.fetch_favicon = true;
  if (AddToHomescreenDataFetcher::IsStopgapEnabled()) {
    // TODO(crbug.com/570202962): `is_debug_mode` bypasses the early exit in
    // `InstallableTask` when `InstallablePageData` already cached
    // `MANIFEST_PARSING_OR_NETWORK_ERROR` in Step 1, allowing Step 2's favicon
    // fallback to run. Replace this workaround when the v2 two-stage fetcher
    // decouples icon fetching from cached manifest errors.
    params.is_debug_mode = true;
  }
  return params;
}

InstallableParams ParamsToPerformInstallableCheck() {
  InstallableParams params;
  params.check_eligibility = true;
  params.installable_criteria =
      AddToHomescreenDataFetcher::IsStopgapEnabled()
          ? InstallableCriteria::kImplicitManifestFieldsHTML
          : InstallableCriteria::kNoManifestAtRootScope;
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

// Codes that mean "the page is fine but the manifest is not promotable". With
// the stopgap enabled these yield WEBAPK_DIY; anything else yields SHORTCUT.
// Deliberately conservative: a status code added later cannot widen DIY
// eligibility.
bool AllErrorsAreClassification(
    const std::vector<InstallableStatusCode>& errors) {
  for (auto error : errors) {
    switch (error) {
      case InstallableStatusCode::NO_MANIFEST:
      case InstallableStatusCode::MANIFEST_PARSING_OR_NETWORK_ERROR:
      case InstallableStatusCode::START_URL_NOT_VALID:
      case InstallableStatusCode::MANIFEST_MISSING_NAME_OR_SHORT_NAME:
      case InstallableStatusCode::MANIFEST_DISPLAY_NOT_SUPPORTED:
      case InstallableStatusCode::MANIFEST_DISPLAY_OVERRIDE_NOT_SUPPORTED:
      case InstallableStatusCode::MANIFEST_MISSING_SUITABLE_ICON:
        break;
      default:
        return false;
    }
  }
  return true;
}

bool OnlyContains(const std::vector<InstallableStatusCode>& errors,
                  std::initializer_list<InstallableStatusCode> allowed) {
  return std::ranges::all_of(errors, [&](InstallableStatusCode error) {
    return std::ranges::contains(allowed, error);
  });
}

// True when `errors` contains one of the codes InstallableManager resets with.
bool ContainsResetError(const std::vector<InstallableStatusCode>& errors) {
  return std::ranges::contains(errors, InstallableStatusCode::USER_NAVIGATED) ||
         std::ranges::contains(errors,
                               InstallableStatusCode::MANIFEST_URL_CHANGED) ||
         std::ranges::contains(errors, InstallableStatusCode::RENDERER_EXITING);
}

bool ArePageAndManifestUrlsEligibleForWebApk(
    const GURL& page_url,
    const blink::mojom::Manifest& manifest) {
  return page_url.SchemeIsHTTPOrHTTPS() && !blink::IsEmptyManifest(manifest) &&
         WebappsUtils::AreWebManifestUrlsWebApkCompatible(manifest);
}

// Decision bucket for a fetch that ends in SHORTCUT because of `data`, shared
// by the flag-off and flag-on arms so the two Finch groups are comparable.
// `arm_specific` is recorded when none of the shared reasons apply.
AddToHomescreenDataFetcher::AnyPageDecision ShortcutDecisionFor(
    const InstallableData& data,
    const GURL& page_url,
    AddToHomescreenDataFetcher::AnyPageDecision arm_specific) {
  if (ContainsResetError(data.errors)) {
    return AddToHomescreenDataFetcher::AnyPageDecision::kShortcutReset;
  }
  if (!ArePageAndManifestUrlsEligibleForWebApk(page_url, *data.manifest) ||
      std::ranges::contains(data.errors, InstallableStatusCode::IN_INCOGNITO) ||
      std::ranges::contains(data.errors,
                            InstallableStatusCode::NOT_FROM_SECURE_ORIGIN)) {
    return AddToHomescreenDataFetcher::AnyPageDecision::kShortcutIneligible;
  }
  return arm_specific;
}

void RecordDecision(AddToHomescreenDataFetcher::AnyPageDecision decision) {
  base::UmaHistogramEnumeration("Webapp.AddToHomescreen.AnyPage.Decision",
                                decision);
}

// Records where a non-SHORTCUT result's primary icon came from. When no
// downloadable icon is available, a monogram icon is generated and given the
// page/start URL as its placeholder `best_primary_icon_url` (for
// WebApkIconsHasher). On phones, `OnIconCreated` sets `best_primary_icon_url`
// to `info.url` (`is_icon_generated == true`). On Android Desktop,
// `InstallableIconFetcher` generates the monogram during Step 2 and sets
// `primary_icon_url` to `web_contents_->GetLastCommittedURL()`, which may
// differ from `info.url` when the manifest specifies a different `start_url`
// or when `GetShortcutUrl` unwraps a distiller URL.
void RecordPrimaryIconSource(const ShortcutInfo& info,
                             const GURL& last_committed_url,
                             bool is_icon_generated) {
  base::UmaHistogramEnumeration(
      "Webapp.AddToHomescreen.AnyPage.PrimaryIconSource",
      is_icon_generated || info.best_primary_icon_url == info.url ||
              info.best_primary_icon_url == last_committed_url
          ? AddToHomescreenDataFetcher::AnyPagePrimaryIconSource::kGenerated
          : AddToHomescreenDataFetcher::AnyPagePrimaryIconSource::kDownloaded);
}

}  // namespace

// static
bool AddToHomescreenDataFetcher::IsStopgapEnabled() {
  return base::FeatureList::IsEnabled(
      features::kAndroidInstallAnyPageAsDiyAppStopgap);
}

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
  PrepareToAddShortcut(AnyPageDecision::kShortcutTimeout);
}

void AddToHomescreenDataFetcher::OnDidGetInstallableData(
    const InstallableData& data) {
  // ~WebContentsImpl notifies observers mid-destruction while weak pointers
  // are still valid; a callback fired by InstallableManager's reset must not
  // issue another GetData against a dying WebContents.
  if (!web_contents_ || web_contents_->IsBeingDestroyed()) {
    return;
  }

  // A reset (navigation, manifest URL change, renderer gone) during round 1,
  // or a crashed/dead main frame whose page data was cached before the crash:
  // do not issue round 2 from inside the callback. This callback can run
  // synchronously inside the constructor, so notify the observer in a
  // separate task.
  if (ContainsResetError(data.errors) || web_contents_->IsCrashed() ||
      !web_contents_->GetPrimaryMainFrame()->IsRenderFrameLive()) {
    StopTimer();
    installable_status_code_ = ContainsResetError(data.errors)
                                   ? data.GetFirstError()
                                   : InstallableStatusCode::RENDERER_EXITING;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&AddToHomescreenDataFetcher::PrepareToAddShortcut,
                       weak_ptr_factory_.GetWeakPtr(),
                       AnyPageDecision::kShortcutReset));
    return;
  }

  RecordMobileCapableUserActions(
      data.web_page_metadata->mobile_capable,
      /*has_manifest=*/!data.manifest_url->is_empty());

  shortcut_info_.UpdateFromWebPageMetadata(*data.web_page_metadata);
  shortcut_info_.UpdateFromManifest(*data.manifest);
  // Keep a 404 / unparsable manifest URL out of ShortcutInfo so the WebAPK
  // update path never fetches it.
  if (!IsStopgapEnabled() ||
      !std::ranges::contains(
          data.errors,
          InstallableStatusCode::MANIFEST_PARSING_OR_NETWORK_ERROR)) {
    shortcut_info_.manifest_url = (*data.manifest_url);
  }
  // Save the splash screen URL for the later download.
  shortcut_info_.UpdateBestSplashIcon(*data.manifest);

  installable_manager_->GetData(
      ParamsToFetchPrimaryIcon(),
      base::BindOnce(&AddToHomescreenDataFetcher::OnDidGetPrimaryIcon,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AddToHomescreenDataFetcher::OnDidGetPrimaryIcon(
    const InstallableData& data) {
  if (!web_contents_ || web_contents_->IsBeingDestroyed()) {
    return;
  }

  if (!data.primary_icon) {
    installable_status_code_ = data.GetFirstError();
    if (!IsStopgapEnabled() ||
        !OnlyContains(
            data.errors,
            {InstallableStatusCode::NO_ACCEPTABLE_ICON,
             InstallableStatusCode::MANIFEST_PARSING_OR_NETWORK_ERROR})) {
      PrepareToAddShortcut(ShortcutDecisionFor(
          data, shortcut_info_.url,
          IsStopgapEnabled() ? AnyPageDecision::kShortcutUnknownError
                             : AnyPageDecision::kShortcutNoIcon));
      return;
    }
    // No usable icon anywhere (InstallableIconFetcher already tried manifest
    // icons, the favicon DB, DOM candidates and <origin>/favicon.ico):
    // classify in round 3 and generate a monogram afterwards.
  } else {
    raw_primary_icon_ = *data.primary_icon;
    shortcut_info_.best_primary_icon_url = (*data.primary_icon_url);
    shortcut_info_.is_primary_icon_maskable = data.has_maskable_primary_icon;
  }

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

  installable_status_code_ = data.GetFirstError();

  if (!IsStopgapEnabled()) {
    bool webapk_compatible =
        (data.errors.empty() && data.installable_check_passed &&
         WebappsUtils::AreWebManifestUrlsWebApkCompatible(*data.manifest));

    if (!webapk_compatible) {
      PrepareToAddShortcut(ShortcutDecisionFor(
          data, shortcut_info_.url, AnyPageDecision::kShortcutNotRootScope));
      return;
    }

    shortcut_info_.UpdateDisplayMode(webapk_compatible);

    AddToHomescreenParams::AppType app_type =
        AddToHomescreenParams::GetWebAppInstallType(
            /*crafted=*/!data.manifest_url->is_empty());

    RecordDecision(app_type == AddToHomescreenParams::AppType::WEBAPK_DIY
                       ? AnyPageDecision::kDiyNoManifest
                       : AnyPageDecision::kCrafted);

    observer_->OnUserTitleAvailable(
        webapk_compatible ? shortcut_info_.name : shortcut_info_.user_title,
        shortcut_info_.url, app_type);

    // WebAPKs should always use the raw icon for the launcher whether or not
    // that icon is maskable.
    primary_icon_ = raw_primary_icon_;
    RecordPrimaryIconSource(shortcut_info_,
                            web_contents_->GetLastCommittedURL(),
                            /*is_icon_generated=*/false);
    // The observer may delete |this| synchronously; nothing below this line.
    observer_->OnDataAvailable(shortcut_info_, primary_icon_, app_type,
                               installable_status_code_);
    return;
  }

  const bool eligible = ArePageAndManifestUrlsEligibleForWebApk(
                            shortcut_info_.url, *data.manifest) &&
                        AllErrorsAreClassification(data.errors);

  if (!eligible) {
    PrepareToAddShortcut(ShortcutDecisionFor(
        data, shortcut_info_.url, AnyPageDecision::kShortcutUnknownError));
    return;
  }

  const bool crafted = data.errors.empty();
  if (crafted) {
    RecordDecision(AnyPageDecision::kCrafted);
  } else if (std::ranges::contains(data.errors,
                                   InstallableStatusCode::NO_MANIFEST)) {
    RecordDecision(AnyPageDecision::kDiyNoManifest);
  } else if (std::ranges::contains(
                 data.errors,
                 InstallableStatusCode::MANIFEST_PARSING_OR_NETWORK_ERROR)) {
    RecordDecision(AnyPageDecision::kDiyManifestError);
  } else {
    RecordDecision(AnyPageDecision::kDiyNotPromotable);
  }

  app_type_ = AddToHomescreenParams::GetWebAppInstallType(crafted);
  shortcut_info_.UpdateDisplayMode(app_type_);

  observer_->OnUserTitleAvailable(
      crafted ? shortcut_info_.name : shortcut_info_.user_title,
      shortcut_info_.url, app_type_);

  if (!raw_primary_icon_.isNull()) {
    primary_icon_ = raw_primary_icon_;
    RecordPrimaryIconSource(shortcut_info_,
                            web_contents_->GetLastCommittedURL(),
                            /*is_icon_generated=*/false);
    // The observer may delete |this| synchronously; nothing below this line.
    observer_->OnDataAvailable(shortcut_info_, primary_icon_, app_type_,
                               installable_status_code_);
  } else {
    CreateIconForView(SkBitmap());
  }
}

void AddToHomescreenDataFetcher::PrepareToAddShortcut(
    AnyPageDecision decision) {
  if (!web_contents_) {
    return;
  }
  RecordDecision(decision);
  app_type_ = AddToHomescreenParams::AppType::SHORTCUT;
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
    shortcut_info_.is_primary_icon_maskable = false;
    // A generated icon has no URL of its own. For a WebAPK (DIY or crafted)
    // record the page URL instead: WebApkIconsHasher only hashes URLs that are
    // valid, and when the download of that URL yields no bitmap it falls back
    // to hashing the generated PNG, so this keeps the icon in the install
    // proto. SHORTCUTs keep an empty URL so nothing is fetched. Note:
    // AddToHomescreenCoordinator may still downgrade a WebAPK to SHORTCUT
    // later (sheet "Add shortcut"); that is safe only because
    // AddToHomescreenMediator forces display=browser for shortcuts, which
    // makes ShortcutHelper.addShortcut ignore the icon URL.
    shortcut_info_.best_primary_icon_url =
        (app_type_ == AddToHomescreenParams::AppType::SHORTCUT
             ? GURL()
             : shortcut_info_.url);
  }

  if (app_type_ != AddToHomescreenParams::AppType::SHORTCUT) {
    RecordPrimaryIconSource(shortcut_info_,
                            web_contents_->GetLastCommittedURL(),
                            is_icon_generated);
  }

  // The observer may delete |this| synchronously (the universal install
  // sheet's AppDataFetcher does); nothing below this line.
  observer_->OnDataAvailable(shortcut_info_, icon_for_view, app_type_,
                             installable_status_code_);
}

}  // namespace webapps
