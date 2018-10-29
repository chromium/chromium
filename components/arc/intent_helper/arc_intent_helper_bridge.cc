// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/intent_helper/arc_intent_helper_bridge.h"

#include <iterator>
#include <utility>

#include "ash/new_window_controller.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "ash/public/interfaces/wallpaper.mojom.h"
#include "ash/shell.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/audio/arc_audio_bridge.h"
#include "components/arc/intent_helper/open_url_delegate.h"
#include "components/url_formatter/url_fixer.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/base/layout.h"
#include "url/url_constants.h"

namespace arc {
namespace {

constexpr std::pair<mojom::ChromePage, const char*> kMapping[] = {
    {mojom::ChromePage::MULTIDEVICE, "multidevice"},
    {mojom::ChromePage::MAIN, ""},
    {mojom::ChromePage::POWER, "power"},
    {mojom::ChromePage::BLUETOOTH, "bluetoothDevices"},
    {mojom::ChromePage::DATETIME, "dateTime"},
    {mojom::ChromePage::DISPLAY, "display"},
    {mojom::ChromePage::WIFI, "networks/?type=WiFi"},
    {mojom::ChromePage::PRIVACY, "privacy"},
    {mojom::ChromePage::HELP, "help"},
    {mojom::ChromePage::ACCOUNTS, "accounts"},
    {mojom::ChromePage::APPEARANCE, "appearance"},
    {mojom::ChromePage::AUTOFILL, "autofill"},
    {mojom::ChromePage::BLUETOOTHDEVICES, "bluetoothDevices"},
    {mojom::ChromePage::CHANGEPICTURE, "changePicture"},
    {mojom::ChromePage::CLEARBROWSERDATA, "clearBrowserData"},
    {mojom::ChromePage::CLOUDPRINTERS, "cloudPrinters"},
    {mojom::ChromePage::CUPSPRINTERS, "cupsPrinters"},
    {mojom::ChromePage::DOWNLOADS, "downloads"},
    {mojom::ChromePage::KEYBOARDOVERLAY, "keyboard-overlay"},
    {mojom::ChromePage::LANGUAGES, "languages"},
    {mojom::ChromePage::LOCKSCREEN, "lockScreen"},
    {mojom::ChromePage::MANAGEACCESSIBILITY, "manageAccessibility"},
    {mojom::ChromePage::NETWORKSTYPEVPN, "networks?type=VPN"},
    {mojom::ChromePage::ONSTARTUP, "onStartup"},
    {mojom::ChromePage::PASSWORDS, "passwords"},
    {mojom::ChromePage::POINTEROVERLAY, "pointer-overlay"},
    {mojom::ChromePage::RESET, "reset"},
    {mojom::ChromePage::SEARCH, "search"},
    {mojom::ChromePage::STORAGE, "storage"},
    {mojom::ChromePage::SYNCSETUP, "syncSetup"},
    {mojom::ChromePage::ABOUTBLANK, url::kAboutBlankURL},
    {mojom::ChromePage::ABOUTDOWNLOADS, "about:downloads"},
    {mojom::ChromePage::ABOUTHISTORY, "about:history"}};

constexpr const char* kArcSchemes[] = {url::kHttpScheme, url::kHttpsScheme,
                                       url::kContentScheme, url::kFileScheme,
                                       url::kMailToScheme};

// mojom::ChromePage::LAST returns the ammout of valid entries - 1.
static_assert(arraysize(kMapping) ==
                  static_cast<size_t>(mojom::ChromePage::LAST) + 1,
              "kMapping is out of sync");

// Not owned. Must outlive all ArcIntentHelperBridge instances. Typically this
// is ChromeNewWindowClient in the browser.
OpenUrlDelegate* g_open_url_delegate = nullptr;

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

// Base URL for the Chrome settings pages.
constexpr char kSettingsPageBaseUrl[] = "chrome://settings";

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

ArcIntentHelperBridge::ArcIntentHelperBridge(content::BrowserContext* context,
                                             ArcBridgeService* bridge_service)
    : context_(context),
      arc_bridge_service_(bridge_service),
      allowed_chrome_pages_map_(std::cbegin(kMapping), std::cend(kMapping)),
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
  // TODO(607411): If the FileManager is not yet open this will open to
  // downloads by default, which is what we want.  However if it is open it will
  // simply be brought to the forgeground without forcibly being navigated to
  // downloads, which is probably not ideal.
  // TODO(mash): Support this functionality without ash::Shell access in Chrome.
  if (ash::Shell::HasInstance())
    ash::Shell::Get()->new_window_controller()->OpenFileManager();
}

void ArcIntentHelperBridge::OnOpenUrl(const std::string& url) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Converts |url| to a fixed-up one and checks validity.
  const GURL gurl(url_formatter::FixupURL(url, /*desired_tld=*/std::string()));
  if (!gurl.is_valid())
    return;

  if (allowed_arc_schemes_.find(gurl.scheme()) != allowed_arc_schemes_.end())
    g_open_url_delegate->OpenUrlFromArc(gurl);
}

void ArcIntentHelperBridge::OnOpenChromePage(mojom::ChromePage page) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto it = allowed_chrome_pages_map_.find(page);
  if (it == allowed_chrome_pages_map_.end()) {
    LOG(WARNING) << "The requested ChromePage is invalid: "
                 << static_cast<int>(page);
    return;
  }

  GURL page_gurl(it->second);
  if (page_gurl.SchemeIs(url::kAboutScheme)) {
    g_open_url_delegate->OpenUrlFromArc(page_gurl);
  } else {
    g_open_url_delegate->OpenUrlFromArc(
        GURL(kSettingsPageBaseUrl).Resolve(it->second));
  }
}

void ArcIntentHelperBridge::OpenWallpaperPicker() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ash::mojom::WallpaperControllerPtr wallpaper_controller_ptr;
  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindInterface(ash::mojom::kServiceName, &wallpaper_controller_ptr);
  wallpaper_controller_ptr->OpenWallpaperPickerIfAllowed();
}

void ArcIntentHelperBridge::SetWallpaperDeprecated(
    const std::vector<uint8_t>& jpeg_data) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  LOG(ERROR) << "IntentHelper.SetWallpaper is deprecated";
}

void ArcIntentHelperBridge::OpenVolumeControl() {
  auto* audio = ArcAudioBridge::GetForBrowserContext(context_);
  DCHECK(audio);
  audio->ShowVolumeControls();
}

void ArcIntentHelperBridge::OnOpenWebApp(const std::string& url) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
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

  for (const IntentFilter& filter : intent_filters_) {
    if (filter.Match(url))
      return false;
  }

  // Didn't find any matches for Android so let Chrome handle it.
  return true;
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
  intent_filters_ = std::move(filters);

  for (auto& observer : observer_list_)
    observer.OnIntentFiltersUpdated();
}

}  // namespace arc
