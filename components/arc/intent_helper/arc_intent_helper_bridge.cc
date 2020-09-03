// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/intent_helper/arc_intent_helper_bridge.h"

#include <iterator>
#include <utility>

#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/wallpaper_controller.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/audio/arc_audio_bridge.h"
#include "components/arc/intent_helper/control_camera_app_delegate.h"
#include "components/arc/intent_helper/factory_reset_delegate.h"
#include "components/arc/intent_helper/open_url_delegate.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/url_formatter/url_fixer.h"
#include "ui/base/layout.h"
#include "url/url_constants.h"

namespace arc {
namespace {

constexpr const char* kArcSchemes[] = {url::kHttpScheme, url::kHttpsScheme,
                                       url::kContentScheme, url::kFileScheme,
                                       url::kMailToScheme};

// Not owned. Must outlive all ArcIntentHelperBridge instances. Typically this
// is ChromeNewWindowClient in the browser.
OpenUrlDelegate* g_open_url_delegate = nullptr;
ControlCameraAppDelegate* g_control_camera_app_delegate = nullptr;
FactoryResetDelegate* g_factory_reset_delegate = nullptr;

// Singleton factory for ArcIntentHelperBridge.
class ArcIntentHelperBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcIntentHelperBridge,
          ArcIntentHelperBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcIntentHelperBridgeFactory";

  static ArcIntentHelperBridgeFactory* GetInstance() {
    return base::Singleton<ArcIntentHelperBridgeFactory>::get();
  }

 private:
  friend struct base::DefaultSingletonTraits<ArcIntentHelperBridgeFactory>;

  ArcIntentHelperBridgeFactory() = default;
  ~ArcIntentHelperBridgeFactory() override = default;
};

// Keep in sync with ArcIntentHelperOpenType enum in
// //tools/metrics/histograms/enums.xml.
enum class ArcIntentHelperOpenType {
  DOWNLOADS = 0,
  URL = 1,
  CUSTOM_TAB = 2,
  WALLPAPER_PICKER = 3,
  VOLUME_CONTROL = 4,
  CHROME_PAGE = 5,
  WEB_APP = 6,
  kMaxValue = WEB_APP,
};

// Records Arc.IntentHelper.OpenType UMA histogram.
void RecordOpenType(ArcIntentHelperOpenType type) {
  UMA_HISTOGRAM_ENUMERATION("Arc.IntentHelper.OpenType", type);
}

}  // namespace

// static
const char ArcIntentHelperBridge::kArcIntentHelperPackageName[] =
    "org.chromium.arc.intent_helper";

// static
ArcIntentHelperBridge* ArcIntentHelperBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcIntentHelperBridgeFactory::GetForBrowserContext(context);
}

// static
KeyedServiceBaseFactory* ArcIntentHelperBridge::GetFactory() {
  return ArcIntentHelperBridgeFactory::GetInstance();
}

// static
std::string ArcIntentHelperBridge::AppendStringToIntentHelperPackageName(
    const std::string& to_append) {
  return base::JoinString({kArcIntentHelperPackageName, to_append}, ".");
}

// static
void ArcIntentHelperBridge::SetOpenUrlDelegate(OpenUrlDelegate* delegate) {
  g_open_url_delegate = delegate;
}

// static
void ArcIntentHelperBridge::SetControlCameraAppDelegate(
    ControlCameraAppDelegate* delegate) {
  g_control_camera_app_delegate = delegate;
}

// static
void ArcIntentHelperBridge::SetFactoryResetDelegate(
    FactoryResetDelegate* delegate) {
  g_factory_reset_delegate = delegate;
}

ArcIntentHelperBridge::ArcIntentHelperBridge(content::BrowserContext* context,
                                             ArcBridgeService* bridge_service)
    : context_(context),
      arc_bridge_service_(bridge_service),
      allowed_arc_schemes_(std::cbegin(kArcSchemes), std::cend(kArcSchemes)) {
  arc_bridge_service_->intent_helper()->SetHost(this);
}

ArcIntentHelperBridge::~ArcIntentHelperBridge() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  arc_bridge_service_->intent_helper()->SetHost(nullptr);
}

void ArcIntentHelperBridge::OnIconInvalidated(const std::string& package_name) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  icon_loader_.InvalidateIcons(package_name);
}

void ArcIntentHelperBridge::OnOpenDownloads() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  RecordOpenType(ArcIntentHelperOpenType::DOWNLOADS);
  ash::NewWindowDelegate::GetInstance()->OpenDownloadsFolder();
}

void ArcIntentHelperBridge::OnOpenUrl(const std::string& url) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  RecordOpenType(ArcIntentHelperOpenType::URL);
  // Converts |url| to a fixed-up one and checks validity.
  const GURL gurl(url_formatter::FixupURL(url, /*desired_tld=*/std::string()));
  if (!gurl.is_valid())
    return;

  if (allowed_arc_schemes_.find(gurl.scheme()) != allowed_arc_schemes_.end())
    g_open_url_delegate->OpenUrlFromArc(gurl);
}

void ArcIntentHelperBridge::OnOpenCustomTab(const std::string& url,
                                            int32_t task_id,
                                            int32_t surface_id,
                                            int32_t top_margin,
                                            OnOpenCustomTabCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  RecordOpenType(ArcIntentHelperOpenType::CUSTOM_TAB);
  // Converts |url| to a fixed-up one and checks validity.
  const GURL gurl(url_formatter::FixupURL(url, /*desired_tld=*/std::string()));
  if (!gurl.is_valid() ||
      allowed_arc_schemes_.find(gurl.scheme()) == allowed_arc_schemes_.end()) {
    std::move(callback).Run(mojo::NullRemote());
    return;
  }
  g_open_url_delegate->OpenArcCustomTab(gurl, task_id, std::move(callback));
}

void ArcIntentHelperBridge::OnOpenChromePage(mojom::ChromePage page) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  RecordOpenType(ArcIntentHelperOpenType::CHROME_PAGE);

  g_open_url_delegate->OpenChromePageFromArc(page);
}

void ArcIntentHelperBridge::FactoryResetArc() {
  g_factory_reset_delegate->ResetArc();
}

void ArcIntentHelperBridge::OpenWallpaperPicker() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  RecordOpenType(ArcIntentHelperOpenType::WALLPAPER_PICKER);
  ash::WallpaperController::Get()->OpenWallpaperPickerIfAllowed();
}

void ArcIntentHelperBridge::SetWallpaperDeprecated(
    const std::vector<uint8_t>& jpeg_data) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  LOG(ERROR) << "IntentHelper.SetWallpaper is deprecated";
}

void ArcIntentHelperBridge::OpenVolumeControl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  RecordOpenType(ArcIntentHelperOpenType::VOLUME_CONTROL);
  auto* audio = ArcAudioBridge::GetForBrowserContext(context_);
  DCHECK(audio);
  audio->ShowVolumeControls();
}

void ArcIntentHelperBridge::OnOpenWebApp(const std::string& url) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  RecordOpenType(ArcIntentHelperOpenType::WEB_APP);
  // Converts |url| to a fixed-up one and checks validity.
  const GURL gurl(url_formatter::FixupURL(url, /*desired_tld=*/std::string()));
  if (!gurl.is_valid())
    return;

  // Web app launches should only be invoked on HTTPS URLs.
  if (gurl.SchemeIs(url::kHttpsScheme))
    g_open_url_delegate->OpenWebAppFromArc(gurl);
}

void ArcIntentHelperBridge::RecordShareFilesMetrics(mojom::ShareFiles flag) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Record metrics coming from ARC, these are related Share files feature
  // stability.
  UMA_HISTOGRAM_ENUMERATION("Arc.ShareFilesOnExit", flag);
}

void ArcIntentHelperBridge::LaunchCameraApp(uint32_t intent_id,
                                            arc::mojom::CameraIntentMode mode,
                                            bool should_handle_result,
                                            bool should_down_scale,
                                            bool is_secure,
                                            int32_t task_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  base::DictionaryValue intent_info;
  std::string mode_str =
      mode == arc::mojom::CameraIntentMode::PHOTO ? "photo" : "video";

  std::stringstream queries;
  queries << "?intentId=" << intent_id << "&mode=" << mode_str
          << "&shouldHandleResult=" << should_handle_result
          << "&shouldDownScale=" << should_down_scale
          << "&isSecure=" << is_secure;
  g_control_camera_app_delegate->LaunchCameraApp(queries.str(), task_id);
}

void ArcIntentHelperBridge::OnIntentFiltersUpdatedForPackage(
    const std::string& package_name,
    std::vector<IntentFilter> filters) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  intent_filters_.erase(package_name);
  if (filters.size() > 0)
    intent_filters_[package_name] = std::move(filters);

  for (auto& observer : observer_list_)
    observer.OnIntentFiltersUpdated(package_name);
}

void ArcIntentHelperBridge::CloseCameraApp() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  g_control_camera_app_delegate->CloseCameraApp();
}

void ArcIntentHelperBridge::IsChromeAppEnabled(
    arc::mojom::ChromeApp app,
    IsChromeAppEnabledCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (app == arc::mojom::ChromeApp::CAMERA) {
    std::move(callback).Run(
        g_control_camera_app_delegate->IsCameraAppEnabled());
    return;
  }

  NOTREACHED() << "Unknown chrome app";
  std::move(callback).Run(false);
}

void ArcIntentHelperBridge::OnPreferredAppsChanged(
    std::vector<IntentFilter> added,
    std::vector<IntentFilter> deleted) {
  added_preferred_apps_ = std::move(added);
  deleted_preferred_apps_ = std::move(deleted);
  for (auto& observer : observer_list_)
    observer.OnPreferredAppsChanged();
}

ArcIntentHelperBridge::GetResult ArcIntentHelperBridge::GetActivityIcons(
    const std::vector<ActivityName>& activities,
    OnIconsReadyCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return icon_loader_.GetActivityIcons(activities, std::move(callback));
}

bool ArcIntentHelperBridge::ShouldChromeHandleUrl(const GURL& url) {
  if (!url.SchemeIsHTTPOrHTTPS()) {
    // Chrome will handle everything that is not http and https.
    return true;
  }

  for (auto& package_filters : intent_filters_) {
    // The intent helper package is used by ARC to send URLs to Chrome, so it
    // does not count as a candidate.
    if (IsIntentHelperPackage(package_filters.first))
      continue;
    for (auto& filter : package_filters.second) {
      if (filter.Match(url))
        return false;
    }
  }

  // Didn't find any matches for Android so let Chrome handle it.
  return true;
}

void ArcIntentHelperBridge::SetAdaptiveIconDelegate(
    AdaptiveIconDelegate* delegate) {
  icon_loader_.SetAdaptiveIconDelegate(delegate);
}

void ArcIntentHelperBridge::AddObserver(ArcIntentHelperObserver* observer) {
  observer_list_.AddObserver(observer);
}

void ArcIntentHelperBridge::RemoveObserver(ArcIntentHelperObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

bool ArcIntentHelperBridge::HasObserver(
    ArcIntentHelperObserver* observer) const {
  return observer_list_.HasObserver(observer);
}

void ArcIntentHelperBridge::HandleCameraResult(
    uint32_t intent_id,
    arc::mojom::CameraIntentAction action,
    const std::vector<uint8_t>& data,
    arc::mojom::IntentHelperInstance::HandleCameraResultCallback callback) {
  auto* arc_service_manager = arc::ArcServiceManager::Get();
  arc::mojom::IntentHelperInstance* instance = nullptr;
  if (arc_service_manager) {
    instance = ARC_GET_INSTANCE_FOR_METHOD(
        arc_service_manager->arc_bridge_service()->intent_helper(),
        HandleCameraResult);
  }
  if (!instance) {
    LOG(ERROR) << "Failed to get instance for HandleCameraResult().";
    std::move(callback).Run(false);
    return;
  }

  instance->HandleCameraResult(intent_id, action, data, std::move(callback));
}

// static
bool ArcIntentHelperBridge::IsIntentHelperPackage(
    const std::string& package_name) {
  return package_name == kArcIntentHelperPackageName;
}

// static
std::vector<mojom::IntentHandlerInfoPtr>
ArcIntentHelperBridge::FilterOutIntentHelper(
    std::vector<mojom::IntentHandlerInfoPtr> handlers) {
  std::vector<mojom::IntentHandlerInfoPtr> handlers_filtered;
  for (auto& handler : handlers) {
    if (IsIntentHelperPackage(handler->package_name))
      continue;
    handlers_filtered.push_back(std::move(handler));
  }
  return handlers_filtered;
}

void ArcIntentHelperBridge::OnIntentFiltersUpdated(
    std::vector<IntentFilter> filters) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  intent_filters_.clear();

  for (auto& filter : filters)
    intent_filters_[filter.package_name()].push_back(std::move(filter));

  for (auto& observer : observer_list_)
    observer.OnIntentFiltersUpdated(base::nullopt);
}

const std::vector<IntentFilter>&
ArcIntentHelperBridge::GetIntentFilterForPackage(
    const std::string& package_name) {
  return intent_filters_[package_name];
}

const std::vector<IntentFilter>&
ArcIntentHelperBridge::GetAddedPreferredApps() {
  return added_preferred_apps_;
}

const std::vector<IntentFilter>&
ArcIntentHelperBridge::GetDeletedPreferredApps() {
  return deleted_preferred_apps_;
}

}  // namespace arc
