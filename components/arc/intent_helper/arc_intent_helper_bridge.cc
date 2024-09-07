// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/intent_helper/arc_intent_helper_bridge.h"

#include <iterator>
#include <utility>

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/audio/arc_audio_bridge.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/system/power/power_button_controller_base.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/arc/common/intent_helper/arc_intent_helper_package.h"
#include "components/arc/intent_helper/control_camera_app_delegate.h"
#include "components/arc/intent_helper/intent_constants.h"
#include "components/arc/intent_helper/open_url_delegate.h"
#include "components/url_formatter/url_fixer.h"
#include "net/base/url_util.h"
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

  static void ShutDownForTesting(content::BrowserContext* context) {
    auto* factory = GetInstance();
    factory->BrowserContextShutdown(context);
    factory->BrowserContextDestroyed(context);
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

// Returns true if a Web App is allowed to be opened for the given URL.
bool CanOpenWebAppForUrl(const GURL& url) {
  bool is_http_localhost =
      url.SchemeIs(url::kHttpScheme) && net::IsLocalhost(url);
  return url.is_valid() &&
         (url.SchemeIs(url::kHttpsScheme) || is_http_localhost);
}

}  // namespace

// static
ArcIntentHelperBridge* ArcIntentHelperBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcIntentHelperBridgeFactory::GetForBrowserContext(context);
}

// static
ArcIntentHelperBridge* ArcIntentHelperBridge::GetForBrowserContextForTesting(
    content::BrowserContext* context) {
  return ArcIntentHelperBridgeFactory::GetForBrowserContextForTesting(context);
}

// static
void ArcIntentHelperBridge::ShutDownForTesting(
    content::BrowserContext* context) {
  return ArcIntentHelperBridgeFactory::ShutDownForTesting(context);
}

// static
BrowserContextKeyedServiceFactory* ArcIntentHelperBridge::GetFactory() {
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

void ArcIntentHelperBridge::SetDelegate(std::unique_ptr<Delegate> delegate) {
  delegate_ = std::move(delegate);
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

void ArcIntentHelperBridge::Shutdown() {
  for (auto& observer : observer_list_) {
    observer.OnArcIntentHelperBridgeShutdown(this);
  }
}

void ArcIntentHelperBridge::OnIconInvalidated(const std::string& package_name) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  icon_loader_.InvalidateIcons(package_name);
  for (auto& observer : observer_list_)
    observer.OnIconInvalidated(package_name);
}

void ArcIntentHelperBridge::OnIntentFiltersUpdated(
    std::vector<IntentFilter> filters) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  intent_filters_.clear();

  for (auto& filter : filters)
    intent_filters_[filter.package_name()].push_back(std::move(filter));

  for (auto& observer : observer_list_)
    observer.OnIntentFiltersUpdated(std::nullopt);
}

void ArcIntentHelperBridge::OnOpenDownloads() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ash::NewWindowDelegate::GetInstance()->OpenDownloadsFolder();
}

void ArcIntentHelperBridge::OnOpenUrl(const std::string& url) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Converts |url| to a fixed-up one and checks validity.
  const GURL gurl(url_formatter::FixupURL(url, /*desired_tld=*/std::string()));
  if (!gurl.is_valid()) {
    return;
  }

  if (allowed_arc_schemes_.find(gurl.scheme()) != allowed_arc_schemes_.end()) {
    g_open_url_delegate->OpenUrlFromArc(gurl);
  }
}

void ArcIntentHelperBridge::OnOpenCustomTab(const std::string& url,
                                            int32_t task_id,
                                            OnOpenCustomTabCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
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

  g_open_url_delegate->OpenChromePageFromArc(page);
}

void ArcIntentHelperBridge::FactoryResetArc() {
  if (delegate_) {
    delegate_->ResetArc();
  }
}

void ArcIntentHelperBridge::OpenWallpaperPicker() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ash::WallpaperController::Get()->OpenWallpaperPickerIfAllowed();
}

void ArcIntentHelperBridge::OpenVolumeControl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto* audio = ArcAudioBridge::GetForBrowserContext(context_);
  DCHECK(audio);
  audio->ShowVolumeControls();
}

void ArcIntentHelperBridge::OnOpenWebApp(const std::string& url) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Converts |url| to a fixed-up one and checks validity.
  const GURL gurl(url_formatter::FixupURL(url, /*desired_tld=*/std::string()));

  // Web app launches should only be invoked on HTTPS URLs.
  if (CanOpenWebAppForUrl(gurl)) {
    g_open_url_delegate->OpenWebAppFromArc(gurl);
  }
}

void ArcIntentHelperBridge::LaunchCameraApp(uint32_t intent_id,
                                            arc::mojom::CameraIntentMode mode,
                                            bool should_handle_result,
                                            bool should_down_scale,
                                            bool is_secure,
                                            int32_t task_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

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
  if (filters.size() > 0) {
    intent_filters_[package_name] = std::move(filters);
  }

  for (auto& observer : observer_list_) {
    observer.OnIntentFiltersUpdated(package_name);
  }
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

  NOTREACHED_IN_MIGRATION() << "Unknown chrome app";
  std::move(callback).Run(false);
}

void ArcIntentHelperBridge::OnSupportedLinksChanged(
    std::vector<arc::mojom::SupportedLinksPackagePtr> added_packages,
    std::vector<arc::mojom::SupportedLinksPackagePtr> removed_packages,
    arc::mojom::SupportedLinkChangeSource source) {
  for (auto& observer : observer_list_)
    observer.OnArcSupportedLinksChanged(added_packages, removed_packages,
                                        source);
}

void ArcIntentHelperBridge::OnDownloadAddedDeprecated(
    const std::string& relative_path_as_string,
    const std::string& owner_package_name) {
  // The `OnDownloadAdded()` event has been broken since at least 01/2022
  // (see crbug.com/1291882). It is being fixed and replaced with a new API,
  // `mojom::FileSystemHost::OnMediaStoreUriAdded()`.
  LOG(ERROR) << "`OnDownloadAdded()` is deprecated.";
}

void ArcIntentHelperBridge::OnOpenAppWithIntent(
    const GURL& start_url,
    arc::mojom::LaunchIntentPtr intent) {
  // Web app launches should only be invoked on HTTPS URLs.
  if (CanOpenWebAppForUrl(start_url)) {
    g_open_url_delegate->OpenAppWithIntent(start_url, std::move(intent));
  }
}

void ArcIntentHelperBridge::OnOpenGlobalActions() {
  ash::PowerButtonControllerBase::Get()->OnArcPowerButtonMenuEvent();
}

void ArcIntentHelperBridge::OnCloseSystemDialogs() {
  ash::PowerButtonControllerBase::Get()->CancelPowerButtonEvent();
}

ArcIntentHelperBridge::GetResult ArcIntentHelperBridge::GetActivityIcons(
    const std::vector<ActivityName>& activities,
    OnIconsReadyCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return icon_loader_.GetActivityIcons(activities, std::move(callback));
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

void ArcIntentHelperBridge::SendNewCaptureBroadcast(bool is_video,
                                                    std::string file_path) {
  auto* arc_service_manager = arc::ArcServiceManager::Get();
  arc::mojom::IntentHelperInstance* instance = nullptr;

  if (arc_service_manager) {
    instance = ARC_GET_INSTANCE_FOR_METHOD(
        arc_service_manager->arc_bridge_service()->intent_helper(),
        SendBroadcast);
  }
  if (!instance) {
    LOG(WARNING) << "Failed to get instance for SendBroadcast().";
    return;
  }

  std::string action =
      is_video ? "org.chromium.arc.intent_helper.ACTION_SEND_NEW_VIDEO"
               : "org.chromium.arc.intent_helper.ACTION_SEND_NEW_PICTURE";
  base::Value::Dict value;
  value.Set("file_path", file_path);
  std::string extras;
  base::JSONWriter::Write(value, &extras);

  instance->SendBroadcast(action, "org.chromium.arc.intent_helper",
                          /*cls=*/std::string(), extras);
}

void ArcIntentHelperBridge::OnAndroidSettingChange(
    arc::mojom::AndroidSetting setting,
    bool is_enabled) {
  if (!delegate_) {
    LOG(ERROR) << "Unable to set value as ARC app delegate is null.";
    return;
  }
  delegate_->HandleUpdateAndroidSettings(setting, is_enabled);
}

// static
std::vector<mojom::IntentHandlerInfoPtr>
ArcIntentHelperBridge::FilterOutIntentHelper(
    std::vector<mojom::IntentHandlerInfoPtr> handlers) {
  std::vector<mojom::IntentHandlerInfoPtr> handlers_filtered;
  for (auto& handler : handlers) {
    if (handler->package_name == kArcIntentHelperPackageName) {
      continue;
    }
    handlers_filtered.push_back(std::move(handler));
  }
  return handlers_filtered;
}

const std::vector<IntentFilter>&
ArcIntentHelperBridge::GetIntentFilterForPackage(
    const std::string& package_name) {
  return intent_filters_[package_name];
}

}  // namespace arc
