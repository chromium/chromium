// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/installable/installable_manager.h"

#include <algorithm>
#include <utility>

#include "base/check_is_test.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "components/favicon/content/large_favicon_provider_getter.h"
#include "components/favicon/core/large_favicon_provider.h"
#include "components/favicon/core/large_icon_worker.h"
#include "components/favicon_base/favicon_types.h"
#include "components/security_state/core/security_state.h"
#include "components/webapps/browser/features.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/webapps_client.h"
#include "components/webapps/common/constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/manifest_icon_downloader.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/common/origin_util.h"
#include "content/public/common/url_constants.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/manifest/manifest_icon_selector.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/webapps/browser/android/webapps_icon_utils.h"
#endif

namespace webapps {

namespace {

// Minimum dimension size in pixels for screenshots.
const int kMinimumScreenshotSizeInPx = 320;

// Maximum dimension size in pixels for screenshots.
const int kMaximumScreenshotSizeInPx = 3840;

// This constant is the icon size on Android (48dp) multiplied by the scale
// factor of a Nexus 5 device (3x). It is the currently advertised minimum icon
// size for triggering banners.
const int kMinimumPrimaryIconSizeInPx = 144;

// Maximum number of screenshots allowed, the rest will be ignored.
const int kMaximumNumOfScreenshots = 8;

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
    return kMinimumPrimaryIconSizeInPx;
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
    return kMinimumPrimaryIconSizeInPx;
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
    return kMinimumPrimaryIconSizeInPx;
#endif
  }
}

struct ImageTypeDetails {
  const char* extension;
  const char* mimetype;
};

constexpr ImageTypeDetails kSupportedImageTypes[] = {
    {".png", "image/png"},
    {".svg", "image/svg+xml"},
    {".webp", "image/webp"},
};

bool IsIconTypeSupported(const blink::Manifest::ImageResource& icon) {
  // The type field is optional. If it isn't present, fall back on checking
  // the src extension.
  if (icon.type.empty()) {
    std::string filename = icon.src.ExtractFileName();
    for (const ImageTypeDetails& details : kSupportedImageTypes) {
      if (base::EndsWith(filename, details.extension,
                         base::CompareCase::INSENSITIVE_ASCII)) {
        return true;
      }
    }
    return false;
  }

  for (const ImageTypeDetails& details : kSupportedImageTypes) {
    if (base::EqualsASCII(icon.type, details.mimetype))
      return true;
  }
  return false;
}

// Returns whether |manifest| specifies an SVG or PNG icon that has
// IconPurpose::ANY, with size >= kMinimumPrimaryIconSizeInPx (or size "any").
bool DoesManifestContainRequiredIcon(const blink::mojom::Manifest& manifest) {
  for (const auto& icon : manifest.icons) {
    if (!IsIconTypeSupported(icon))
      continue;

    if (!base::Contains(icon.purpose, IconPurpose::ANY))
      continue;

    for (const auto& size : icon.sizes) {
      if (size.IsEmpty())  // "any"
        return true;
      if (size.width() >= kMinimumPrimaryIconSizeInPx &&
          size.height() >= kMinimumPrimaryIconSizeInPx &&
          size.width() <= InstallableManager::kMaximumIconSizeInPx &&
          size.height() <= InstallableManager::kMaximumIconSizeInPx) {
        return true;
      }
    }
  }

  return false;
}

bool ShouldRejectDisplayMode(blink::mojom::DisplayMode display_mode) {
  return !(display_mode == blink::mojom::DisplayMode::kStandalone ||
           display_mode == blink::mojom::DisplayMode::kFullscreen ||
           display_mode == blink::mojom::DisplayMode::kMinimalUi ||
           display_mode == blink::mojom::DisplayMode::kWindowControlsOverlay ||
           (display_mode == blink::mojom::DisplayMode::kBorderless &&
            base::FeatureList::IsEnabled(blink::features::kWebAppBorderless)) ||
           (display_mode == blink::mojom::DisplayMode::kTabbed &&
            base::FeatureList::IsEnabled(::features::kDesktopPWAsTabStrip)));
}

void OnDidCompleteGetAllErrors(
    base::OnceCallback<void(std::vector<content::InstallabilityError>
                                installability_errors)> callback,
    const InstallableData& data) {
  std::vector<content::InstallabilityError> installability_errors;
  for (auto error : data.errors) {
    content::InstallabilityError installability_error =
        GetInstallabilityError(error);
    if (!installability_error.error_id.empty())
      installability_errors.push_back(installability_error);
  }

  std::move(callback).Run(std::move(installability_errors));
}

void OnDidCompleteGetPrimaryIcon(
    base::OnceCallback<void(const SkBitmap*)> callback,
    const InstallableData& data) {
  std::move(callback).Run(data.primary_icon.get());
}

}  // namespace

namespace test {
int g_minimum_favicon_size_for_testing = 0;
}

InstallableManager::EligiblityProperty::EligiblityProperty() = default;

InstallableManager::EligiblityProperty::~EligiblityProperty() = default;

InstallableManager::ValidManifestProperty::ValidManifestProperty() = default;

InstallableManager::ValidManifestProperty::~ValidManifestProperty() = default;

InstallableManager::IconProperty::IconProperty() = default;

InstallableManager::IconProperty::IconProperty(IconProperty&& other) noexcept =
    default;

InstallableManager::IconProperty::~IconProperty() = default;

InstallableManager::IconProperty& InstallableManager::IconProperty::operator=(
    InstallableManager::IconProperty&& other) = default;

InstallableManager::InstallableManager(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<InstallableManager>(*web_contents),
      eligibility_(std::make_unique<EligiblityProperty>()),
      manifest_(std::make_unique<ManifestProperty>()),
      valid_manifest_(std::make_unique<ValidManifestProperty>()),
      worker_(std::make_unique<ServiceWorkerProperty>()),
      primary_icon_(std::make_unique<IconProperty>()),
      service_worker_context_(nullptr),
      sequenced_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {
  // This is null in unit tests.
  if (web_contents) {
    content::StoragePartition* storage_partition =
        web_contents->GetBrowserContext()->GetStoragePartition(
            web_contents->GetSiteInstance());
    DCHECK(storage_partition);

    service_worker_context_ = storage_partition->GetServiceWorkerContext();
    service_worker_context_->AddObserver(this);
  }
}

InstallableManager::~InstallableManager() {
  if (service_worker_context_)
    service_worker_context_->RemoveObserver(this);
}

// static
int InstallableManager::GetMinimumIconSizeInPx() {
  return kMinimumPrimaryIconSizeInPx;
}

// static
bool InstallableManager::IsContentSecure(content::WebContents* web_contents) {
  if (!web_contents)
    return false;

  // chrome:// URLs are considered secure.
  const GURL& url = web_contents->GetLastCommittedURL();
  if (url.scheme() == content::kChromeUIScheme)
    return true;

  // chrome-untrusted:// URLs are shipped with Chrome, so they are considered
  // secure in this context.
  if (url.scheme() == content::kChromeUIUntrustedScheme)
    return true;

  if (IsOriginConsideredSecure(url))
    return true;

  // This can be null in unit tests but should be non-null in production.
  if (!webapps::WebappsClient::Get())
    return false;

  return security_state::IsSslCertificateValid(
      WebappsClient::Get()->GetSecurityLevelForWebContents(web_contents));
}

// static
bool InstallableManager::IsOriginConsideredSecure(const GURL& url) {
  auto origin = url::Origin::Create(url);
  auto* webapps_client = webapps::WebappsClient::Get();
  return (webapps_client && webapps_client->IsOriginConsideredSecure(origin)) ||
         net::IsLocalhost(url) ||
         network::SecureOriginAllowlist::GetInstance().IsOriginAllowlisted(
             origin);
}

void InstallableManager::GetData(const InstallableParams& params,
                                 InstallableCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(callback);

  // Return immediately if we're already working on a task. The new task will be
  // looked at once the current task is finished.
  bool was_active = task_queue_.HasCurrent();
  task_queue_.Add({params, std::move(callback)});
  if (was_active)
    return;

  WorkOnTask();
}

void InstallableManager::GetAllErrors(
    base::OnceCallback<void(std::vector<content::InstallabilityError>
                                installability_errors)> callback) {
  DCHECK(callback);
  InstallableParams params;
  params.check_eligibility = true;
  params.valid_manifest = true;
  params.check_webapp_manifest_display = true;
  params.fetch_screenshots = true;
  params.valid_primary_icon = true;
  params.is_debug_mode = true;
  GetData(params,
          base::BindOnce(OnDidCompleteGetAllErrors, std::move(callback)));
}

void InstallableManager::GetPrimaryIcon(
    base::OnceCallback<void(const SkBitmap*)> callback) {
  DCHECK(callback);
  InstallableParams params;
  params.valid_primary_icon = true;
  GetData(params,
          base::BindOnce(OnDidCompleteGetPrimaryIcon, std::move(callback)));
}

void InstallableManager::SetSequencedTaskRunnerForTesting(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  sequenced_task_runner_ = task_runner;
}

InstallableManager::ManifestProperty::ManifestProperty() = default;
InstallableManager::ManifestProperty::~ManifestProperty() = default;

std::vector<InstallableStatusCode> InstallableManager::GetErrors(
    const InstallableParams& params) {
  std::vector<InstallableStatusCode> errors;

  if (params.check_eligibility && !eligibility_->errors.empty()) {
    errors.insert(errors.end(), eligibility_->errors.begin(),
                  eligibility_->errors.end());
  }

  if (manifest_->error != NO_ERROR_DETECTED)
    errors.push_back(manifest_->error);

  if (params.valid_manifest && !valid_manifest_->errors.empty()) {
    errors.insert(errors.end(), valid_manifest_->errors.begin(),
                  valid_manifest_->errors.end());
  }

  if (params.has_worker && worker_->error != NO_ERROR_DETECTED)
    errors.push_back(worker_->error);

  if (params.valid_primary_icon && primary_icon_->error != NO_ERROR_DETECTED) {
    if (!params.fetch_favicon || favicon_fetched_) {
      errors.push_back(primary_icon_->error);
    }
  }

  return errors;
}

InstallableStatusCode InstallableManager::eligibility_error() const {
  return eligibility_->errors.empty() ? NO_ERROR_DETECTED
                                      : eligibility_->errors[0];
}

InstallableStatusCode InstallableManager::manifest_error() const {
  return manifest_->error;
}

InstallableStatusCode InstallableManager::valid_manifest_error() const {
  return valid_manifest_->errors.empty() ? NO_ERROR_DETECTED
                                         : valid_manifest_->errors[0];
}

void InstallableManager::set_valid_manifest_error(
    InstallableStatusCode error_code) {
  valid_manifest_->errors.clear();
  if (error_code != NO_ERROR_DETECTED)
    valid_manifest_->errors.push_back(error_code);
}

InstallableStatusCode InstallableManager::worker_error() const {
  return worker_->error;
}

InstallableStatusCode InstallableManager::icon_error() {
  return primary_icon_->error;
}

GURL& InstallableManager::icon_url() {
  return primary_icon_->url;
}

const SkBitmap* InstallableManager::icon() {
  return primary_icon_->icon.get();
}

content::WebContents* InstallableManager::GetWebContents() {
  content::WebContents* contents = web_contents();
  if (!contents || contents->IsBeingDestroyed())
    return nullptr;
  return contents;
}

bool InstallableManager::IsComplete(const InstallableParams& params) const {
  // Returns true if for all resources:
  //  a. the params did not request it, OR
  //  b. the resource has been fetched/checked.
  return (!params.check_eligibility || eligibility_->fetched) &&
         manifest_->fetched &&
         (!params.valid_manifest || valid_manifest_->fetched) &&
         (!params.has_worker || worker_->fetched) &&
         (!params.fetch_screenshots || is_screenshots_fetch_complete_) &&
         (!params.valid_primary_icon || primary_icon_->fetched) &&
         (!params.fetch_favicon || favicon_fetched_);
}

void InstallableManager::Reset(InstallableStatusCode error) {
  DCHECK(error != NO_ERROR_DETECTED);
  // Prevent any outstanding callbacks to or from this object from being called.
  weak_factory_.InvalidateWeakPtrs();
  downloaded_screenshots_.clear();
  screenshots_.clear();
  screenshots_downloading_ = 0;
  is_screenshots_fetch_complete_ = false;
  favicon_fetched_ = false;
  favicon_task_tracker_.TryCancelAll();

  // If we have paused tasks, we are waiting for a service worker. Execute the
  // callbacks with the status_code being passed for the paused tasks.
  task_queue_.ResetWithError(error);

  eligibility_ = std::make_unique<EligiblityProperty>();
  manifest_ = std::make_unique<ManifestProperty>();
  valid_manifest_ = std::make_unique<ValidManifestProperty>();
  worker_ = std::make_unique<ServiceWorkerProperty>();
  primary_icon_ = std::make_unique<IconProperty>();

  OnResetData();
}

void InstallableManager::SetManifestDependentTasksComplete() {
  valid_manifest_->fetched = true;
  worker_->fetched = true;
  primary_icon_->fetched = true;
  is_screenshots_fetch_complete_ = true;
}

void InstallableManager::CleanupAndStartNextTask() {
  // Sites can always register a service worker after we finish checking, so
  // don't cache a missing service worker error to ensure we always check
  // again.
  if (worker_error() == NO_MATCHING_SERVICE_WORKER)
    worker_ = std::make_unique<ServiceWorkerProperty>();

  // |valid_manifest_| shouldn't be re-used across tasks because its state is
  // dependent on current task's |params|.
  valid_manifest_ = std::make_unique<ValidManifestProperty>();
  if (manifest_->error == NO_MANIFEST || manifest_->error == MANIFEST_EMPTY) {
    valid_manifest_->fetched = true;
    valid_manifest_->is_valid = false;
  }

  task_queue_.Next();
  WorkOnTask();
}

void InstallableManager::RunCallback(
    InstallableTask task,
    std::vector<InstallableStatusCode> errors) {
  const InstallableParams& params = task.params;

  bool worker_check_passed = worker_->has_worker || !params.has_worker;

  InstallableData data = {
      std::move(errors),
      manifest_url(),
      manifest(),
      primary_icon_->url,
      primary_icon_->icon.get(),
      primary_icon_->purpose == IconPurpose::MASKABLE,
      screenshots_,
      valid_manifest_->is_valid,
      worker_check_passed,
  };

  std::move(task.callback).Run(data);
}

void InstallableManager::WorkOnTask() {
  if (!task_queue_.HasCurrent())
    return;

  const InstallableParams& params = task_queue_.Current().params;

  auto errors = GetErrors(params);
  bool check_passed = errors.empty() || (errors.size() == 1 &&
                                         errors[0] == WARN_NOT_OFFLINE_CAPABLE);
  if ((!check_passed && !params.is_debug_mode) || IsComplete(params)) {
    // Yield the UI thread before processing the next task. If this object is
    // deleted in the meantime, the next task naturally won't run.
    sequenced_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&InstallableManager::CleanupAndStartNextTask,
                                  weak_factory_.GetWeakPtr()));

    auto task = std::move(task_queue_.Current());
    RunCallback(std::move(task), std::move(errors));
    return;
  }

  if (params.check_eligibility && !eligibility_->fetched) {
    CheckEligiblity();
  } else if (!manifest_->fetched) {
    FetchManifest();
  } else if (params.valid_manifest && !valid_manifest_->fetched) {
    CheckManifestValid(params.check_webapp_manifest_display);
  } else if (params.valid_primary_icon && !primary_icon_->fetched) {
    CheckAndFetchBestPrimaryIcon(params.prefer_maskable_icon);
  } else if (params.fetch_favicon && !favicon_fetched_) {
    FetchFavicon();
  } else if (params.fetch_screenshots && !screenshots_downloading_ &&
             !is_screenshots_fetch_complete_) {
    CheckAndFetchScreenshots();
  } else if (params.has_worker && !worker_->fetched) {
    CheckServiceWorker();
  } else {
    NOTREACHED();
  }
}

void InstallableManager::CheckEligiblity() {
  // Fail if this is an incognito window or insecure context.
  content::WebContents* web_contents = GetWebContents();
  if (web_contents->GetBrowserContext()->IsOffTheRecord()) {
    eligibility_->errors.push_back(IN_INCOGNITO);
  }
  if (!IsContentSecure(web_contents)) {
    eligibility_->errors.push_back(NOT_FROM_SECURE_ORIGIN);
  }

  eligibility_->fetched = true;
  WorkOnTask();
}

void InstallableManager::FetchManifest() {
  DCHECK(!manifest_->fetched);

  content::WebContents* web_contents = GetWebContents();
  DCHECK(web_contents);

  // This uses DidFinishNavigation to abort when the primary page changes.
  // Therefore this should always be the correct page.
  web_contents->GetPrimaryPage().GetManifest(base::BindOnce(
      &InstallableManager::OnDidGetManifest, weak_factory_.GetWeakPtr()));
}

void InstallableManager::OnDidGetManifest(const GURL& manifest_url,
                                          blink::mojom::ManifestPtr manifest) {
  if (!GetWebContents())
    return;

  if (manifest_url.is_empty()) {
    manifest_->error = NO_MANIFEST;
    SetManifestDependentTasksComplete();
  } else if (blink::IsEmptyManifest(manifest)) {
    manifest_->error = MANIFEST_EMPTY;
    SetManifestDependentTasksComplete();
  }

  manifest_->url = manifest_url;
  manifest_->manifest = std::move(manifest);
  manifest_->fetched = true;
  WorkOnTask();
}

void InstallableManager::CheckManifestValid(
    bool check_webapp_manifest_display) {
  DCHECK(!valid_manifest_->fetched);
  DCHECK(!blink::IsEmptyManifest(manifest()));

  valid_manifest_->is_valid =
      IsManifestValidForWebApp(manifest(), check_webapp_manifest_display);
  valid_manifest_->fetched = true;
  WorkOnTask();
}

bool InstallableManager::IsManifestValidForWebApp(
    const blink::mojom::Manifest& manifest,
    bool check_webapp_manifest_display) {
  bool is_valid = true;
  if (blink::IsEmptyManifest(manifest)) {
    valid_manifest_->errors.push_back(MANIFEST_EMPTY);
    return false;
  }

  if (!manifest.start_url.is_valid()) {
    valid_manifest_->errors.push_back(START_URL_NOT_VALID);
    is_valid = false;
  } else {
    // If the start_url is valid, the id must be valid.
    CHECK(manifest.id.is_valid());
  }

  if ((!manifest.name || manifest.name->empty()) &&
      (!manifest.short_name || manifest.short_name->empty())) {
    valid_manifest_->errors.push_back(MANIFEST_MISSING_NAME_OR_SHORT_NAME);
    is_valid = false;
  }

  if (check_webapp_manifest_display) {
    blink::mojom::DisplayMode display_mode_to_evaluate = manifest.display;
    InstallableStatusCode manifest_error = MANIFEST_DISPLAY_NOT_SUPPORTED;

    // Unsupported values are ignored when we parse the manifest, and
    // consequently aren't in the manifest.display_override array.
    // If this array is not empty, the first value will "win", so validate
    // this value is installable.
    if (!manifest.display_override.empty()) {
      display_mode_to_evaluate = manifest.display_override[0];
      manifest_error = MANIFEST_DISPLAY_OVERRIDE_NOT_SUPPORTED;
    }

    if (ShouldRejectDisplayMode(display_mode_to_evaluate)) {
      valid_manifest_->errors.push_back(manifest_error);
      is_valid = false;
    }
  }

  if (!DoesManifestContainRequiredIcon(manifest)) {
    valid_manifest_->errors.push_back(MANIFEST_MISSING_SUITABLE_ICON);
    is_valid = false;
  }

  return is_valid;
}

void InstallableManager::CheckServiceWorker() {
  DCHECK(!worker_->fetched);
  DCHECK(!blink::IsEmptyManifest(manifest()));

  if (!service_worker_context_)
    return;

  // Check to see if there is a service worker for the manifest's scope.
  service_worker_context_->CheckHasServiceWorker(
      manifest().scope,
      blink::StorageKey::CreateFirstParty(
          url::Origin::Create(manifest().scope)),
      base::BindOnce(&InstallableManager::OnDidCheckHasServiceWorker,
                     weak_factory_.GetWeakPtr(), base::TimeTicks::Now()));
}

void InstallableManager::OnDidCheckHasServiceWorker(
    base::TimeTicks check_service_worker_start_time,
    content::ServiceWorkerCapability capability) {
  if (!GetWebContents())
    return;

  switch (capability) {
    case content::ServiceWorkerCapability::SERVICE_WORKER_WITH_FETCH_HANDLER:
      if (base::FeatureList::IsEnabled(
              blink::features::kCheckOfflineCapability)) {
        const bool enforce_offline_capability =
            (blink::features::kCheckOfflineCapabilityParam.Get() ==
             blink::features::CheckOfflineCapabilityMode::kEnforce);

        if (!manifest().start_url.is_valid()) {
          worker_->has_worker = false;
          worker_->error = NO_URL_FOR_SERVICE_WORKER;
          worker_->fetched = true;
          WorkOnTask();
          return;
        }

        // Dispatch a fetch event to `start_url` while simulating an offline
        // environment and see if the site supports an offline page.
        service_worker_context_->CheckOfflineCapability(
            manifest().start_url,
            blink::StorageKey::CreateFirstParty(
                url::Origin::Create(manifest().start_url)),
            base::BindOnce(&InstallableManager::OnDidCheckOfflineCapability,
                           weak_factory_.GetWeakPtr(),
                           check_service_worker_start_time,
                           enforce_offline_capability));
        return;
      }
      worker_->has_worker = true;
      break;
    case content::ServiceWorkerCapability::SERVICE_WORKER_NO_FETCH_HANDLER:
      worker_->has_worker = false;
      worker_->error = NOT_OFFLINE_CAPABLE;
      break;
    case content::ServiceWorkerCapability::NO_SERVICE_WORKER:
      InstallableTask& task = task_queue_.Current();
      if (task.params.wait_for_worker) {
        // Wait for ServiceWorkerContextObserver::OnRegistrationCompleted. Set
        // the param |wait_for_worker| to false so we only wait once per task.
        task.params.wait_for_worker = false;
        OnWaitingForServiceWorker();
        task_queue_.PauseCurrent();
        WorkOnTask();
        return;
      }
      worker_->has_worker = false;
      worker_->error = NO_MATCHING_SERVICE_WORKER;
      break;
  }

  // These are recorded in OnDidCheckOfflineCapability() when
  // CheckOfflineCapability is enabled.
  if (!base::FeatureList::IsEnabled(blink::features::kCheckOfflineCapability)) {
    InstallableMetrics::RecordCheckServiceWorkerTime(
        base::TimeTicks::Now() - check_service_worker_start_time);
    InstallableMetrics::RecordCheckServiceWorkerStatus(
        InstallableMetrics::ConvertFromServiceWorkerCapability(capability));
  }

  worker_->fetched = true;
  WorkOnTask();
}

void InstallableManager::OnDidCheckOfflineCapability(
    base::TimeTicks check_service_worker_start_time,
    bool enforce_offline_capability,
    content::OfflineCapability capability,
    int64_t service_worker_registration_id) {
  InstallableMetrics::RecordCheckServiceWorkerTime(
      base::TimeTicks::Now() - check_service_worker_start_time);
  InstallableMetrics::RecordCheckServiceWorkerStatus(
      InstallableMetrics::ConvertFromOfflineCapability(capability));

  switch (capability) {
    case content::OfflineCapability::kSupported:
      worker_->has_worker = true;
      break;
    case content::OfflineCapability::kUnsupported:
      if (enforce_offline_capability) {
        worker_->has_worker = false;
        worker_->error = NOT_OFFLINE_CAPABLE;
      } else {
        // No enforcement means that we are just recording metrics and logging a
        // warning.
        worker_->has_worker = true;
        worker_->error = WARN_NOT_OFFLINE_CAPABLE;
        LogToConsole(web_contents(), WARN_NOT_OFFLINE_CAPABLE,
                     blink::mojom::ConsoleMessageLevel::kWarning);
      }
      break;
  }

  worker_->fetched = true;
  WorkOnTask();
}

void InstallableManager::CheckAndFetchBestPrimaryIcon(bool prefer_maskable) {
  DCHECK(!blink::IsEmptyManifest(manifest()));

  primary_icon_->fetched = true;

  downloading_icons_type_.push_back(IconPurpose::ANY);
  if (prefer_maskable) {
    downloading_icons_type_.push_back(IconPurpose::MASKABLE);
  }

  // Initialize the error to "NO_ACCEPTABLE_ICON". It'll be set to
  // "NO_ERROR_DETECTED" if fetched successfully.
  primary_icon_->error = NO_ACCEPTABLE_ICON;

  TryFetchingNextIcon();
}

void InstallableManager::TryFetchingNextIcon() {
  while (!downloading_icons_type_.empty()) {
    IconPurpose purpose = downloading_icons_type_.back();
    downloading_icons_type_.pop_back();

    GURL icon_url = blink::ManifestIconSelector::FindBestMatchingSquareIcon(
        manifest().icons, GetIdealPrimaryIconSizeInPx(purpose),
        GetMinimumPrimaryIconSizeInPx(purpose), purpose);

    if (icon_url.is_empty()) {
      continue;
    }

    bool can_download_icon = content::ManifestIconDownloader::Download(
        GetWebContents(), icon_url, GetIdealPrimaryIconSizeInPx(purpose),
        GetMinimumPrimaryIconSizeInPx(purpose),
        InstallableManager::kMaximumIconSizeInPx,
        base::BindOnce(&InstallableManager::OnIconFetched,
                       weak_factory_.GetWeakPtr(), icon_url, purpose));
    if (can_download_icon) {
      return;
    }
  }
  WorkOnTask();
}

void InstallableManager::OnIconFetched(const GURL icon_url,
                                       const IconPurpose purpose,
                                       const SkBitmap& bitmap) {
  if (!GetWebContents())
    return;

  if (bitmap.drawsNothing()) {
    primary_icon_->error = NO_ICON_AVAILABLE;
    TryFetchingNextIcon();
    return;
  }

  primary_icon_->url = icon_url;
  primary_icon_->purpose = purpose;
  primary_icon_->icon = std::make_unique<SkBitmap>(bitmap);
  primary_icon_->error = NO_ERROR_DETECTED;
  WorkOnTask();
}

void InstallableManager::FetchFavicon() {
  favicon_fetched_ = true;

  // If primary icon is already successfully fetched, don't fetch favicon.
  if (primary_icon_->fetched && primary_icon_->error == NO_ERROR_DETECTED) {
    WorkOnTask();
    return;
  }

  favicon::LargeFaviconProvider* favicon_provider =
      favicon::GetLargeFaviconProvider(GetWebContents()->GetBrowserContext());
  if (!favicon_provider) {
    WorkOnTask();
    return;
  }

  favicon_provider->GetLargeIconImageOrFallbackStyleForPageUrl(
      GetWebContents()->GetLastCommittedURL(),
      GetMinimumFaviconForPrimaryIconSizeInPx(),
      GetIdealPrimaryIconSizeInPx(IconPurpose::ANY),
      base::BindOnce(&InstallableManager::OnFaviconFetched,
                     weak_factory_.GetWeakPtr()),
      &favicon_task_tracker_);
}

void InstallableManager::OnFaviconFetched(
    const favicon_base::LargeIconImageResult& image_result) {
  if (!GetWebContents()) {
    return;
  }
  // TODO(crbug.com/1462726): add histogram to record fetched favicon size.
  if (!image_result.image.IsEmpty()) {
    primary_icon_->url = image_result.icon_url;
    primary_icon_->icon =
        std::make_unique<SkBitmap>(*image_result.image.ToSkBitmap());
    primary_icon_->purpose = IconPurpose::ANY;
    primary_icon_->error = NO_ERROR_DETECTED;
  }

  WorkOnTask();
}

void InstallableManager::CheckAndFetchScreenshots() {
  DCHECK(!blink::IsEmptyManifest(manifest()));
  DCHECK(!is_screenshots_fetch_complete_);

  screenshots_downloading_ = 0;

  int num_of_screenshots = 0;
  for (const auto& url : manifest().screenshots) {
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

    if (++num_of_screenshots > kMaximumNumOfScreenshots)
      break;

    // A screenshot URL that's in the map is already taken care of.
    if (downloaded_screenshots_.count(url->image.src) > 0)
      continue;

    int ideal_size_in_px = url->image.sizes.empty()
                               ? kMinimumScreenshotSizeInPx
                               : std::max(url->image.sizes[0].width(),
                                          url->image.sizes[0].height());
    // Do not pass in a maximum icon size so that screenshots larger than
    // kMaximumScreenshotSizeInPx are not downscaled to the maximum size by
    // `ManifestIconDownloader::Download`. Screenshots with size larger than
    // kMaximumScreenshotSizeInPx get filtered out by OnScreenshotFetched.
    bool can_download = content::ManifestIconDownloader::Download(
        GetWebContents(), url->image.src, ideal_size_in_px,
        kMinimumScreenshotSizeInPx,
        /*maximum_icon_size_in_px=*/0,
        base::BindOnce(&InstallableManager::OnScreenshotFetched,
                       weak_factory_.GetWeakPtr(), url->image.src),
        /*square_only=*/false);
    if (can_download)
      ++screenshots_downloading_;
  }

  if (!screenshots_downloading_) {
    is_screenshots_fetch_complete_ = true;
    WorkOnTask();
  }
}

void InstallableManager::OnScreenshotFetched(GURL screenshot_url,
                                             const SkBitmap& bitmap) {
  DCHECK_GT(screenshots_downloading_, 0);

  if (!GetWebContents())
    return;

  if (!bitmap.drawsNothing())
    downloaded_screenshots_[screenshot_url] = bitmap;

  if (--screenshots_downloading_ == 0) {
    // Now that all images have finished downloading, populate screenshots in
    // the order they are declared in the manifest.
    int num_of_screenshots = 0;
    for (const auto& url : manifest().screenshots) {
      if (++num_of_screenshots > kMaximumNumOfScreenshots)
        break;

      auto iter = downloaded_screenshots_.find(url->image.src);
      if (iter == downloaded_screenshots_.end())
        continue;

      auto screenshot = iter->second;
      if (screenshot.dimensions().width() > kMaximumScreenshotSizeInPx ||
          screenshot.dimensions().height() > kMaximumScreenshotSizeInPx) {
        continue;
      }

      // Screenshots must have the same aspect ratio. Cross-multiplying
      // dimensions checks portrait vs landscape mode (1:2 vs 2:1 for instance).
      if (screenshots_.size() &&
          screenshot.dimensions().width() *
                  screenshots_[0].image.dimensions().height() !=
              screenshot.dimensions().height() *
                  screenshots_[0].image.dimensions().width()) {
        continue;
      }

      std::pair<int, int> dimensions =
          std::minmax(screenshot.width(), screenshot.height());
      if (dimensions.second > dimensions.first * kMaximumScreenshotRatio)
        continue;

      screenshots_.emplace_back(std::move(screenshot), url->label);
    }

    downloaded_screenshots_.clear();
    is_screenshots_fetch_complete_ = true;

    WorkOnTask();
  }
}

void InstallableManager::OnRegistrationCompleted(const GURL& pattern) {
  // If the scope doesn't match we keep waiting.
  if (!content::ServiceWorkerContext::ScopeMatches(pattern, manifest().scope))
    return;

  bool was_active = task_queue_.HasCurrent();

  // The existence of paused tasks implies that we are waiting for a service
  // worker. We move any paused tasks back into the main queue so that the
  // pipeline will call CheckHasServiceWorker again, in order to find out if
  // the SW has a fetch handler.
  // NOTE: If there are no paused tasks, that means:
  //   a) we've already failed the check, or
  //   b) we haven't yet called CheckHasServiceWorker.
  task_queue_.UnpauseAll();
  if (was_active)
    return;  // If the pipeline was already running, we don't restart it.

  WorkOnTask();
}

void InstallableManager::OnDestruct(content::ServiceWorkerContext* context) {
  service_worker_context_->RemoveObserver(this);
  service_worker_context_ = nullptr;
}

void InstallableManager::PrimaryPageChanged(content::Page& page) {
  Reset(USER_NAVIGATED);
}

void InstallableManager::DidUpdateWebManifestURL(content::RenderFrameHost* rfh,
                                                 const GURL& manifest_url) {
  // A change in the manifest URL invalidates our entire internal state.
  Reset(MANIFEST_URL_CHANGED);
}

void InstallableManager::WebContentsDestroyed() {
  // This ensures that we do not just hang callbacks on web_contents being
  // destroyed.
  Reset(RENDERER_EXITING);
  Observe(nullptr);
}

const GURL& InstallableManager::manifest_url() const {
  return manifest_->url;
}

const blink::mojom::Manifest& InstallableManager::manifest() const {
  DCHECK(manifest_->manifest);
  return *manifest_->manifest;
}

bool InstallableManager::valid_manifest() {
  return valid_manifest_->is_valid;
}

bool InstallableManager::has_worker() {
  return worker_->has_worker;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(InstallableManager);

}  // namespace webapps
