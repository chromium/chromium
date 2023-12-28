// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_INTENT_HELPER_ARC_INTENT_HELPER_BRIDGE_H_
#define COMPONENTS_ARC_INTENT_HELPER_ARC_INTENT_HELPER_BRIDGE_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "ash/components/arc/mojom/intent_helper.mojom.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "components/arc/common/intent_helper/arc_icon_cache_delegate.h"
#include "components/arc/intent_helper/arc_intent_helper_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/gurl.h"

class BrowserContextKeyedServiceFactory;

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class AdaptiveIconDelegate;
class ArcBridgeService;
class ControlCameraAppDelegate;
class IntentFilter;
class OpenUrlDelegate;

// Receives intents from ARC.
class ArcIntentHelperBridge : public KeyedService,
                              public mojom::IntentHelperHost,
                              public ArcIconCacheDelegate {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Resets ARC; this wipes all user data, stops ARC, then
    // re-enables ARC.
    virtual void ResetArc() = 0;

    // Handles Android settings to sync GeoLocation information.
    virtual void HandleUpdateAndroidSettings(mojom::AndroidSetting setting,
                                             bool is_enabled) = 0;
  };
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcIntentHelperBridge* GetForBrowserContext(
      content::BrowserContext* context);
  static ArcIntentHelperBridge* GetForBrowserContextForTesting(
      content::BrowserContext* context);

  static void ShutDownForTesting(content::BrowserContext* context);

  // Returns factory for the ArcIntentHelperBridge.
  static BrowserContextKeyedServiceFactory* GetFactory();

  // Appends '.' + |to_append| to the intent helper package name.
  static std::string AppendStringToIntentHelperPackageName(
      const std::string& to_append);

  static void SetOpenUrlDelegate(OpenUrlDelegate* delegate);

  static void SetControlCameraAppDelegate(ControlCameraAppDelegate* delegate);

  // Sets the Delegate instance.
  void SetDelegate(std::unique_ptr<Delegate> delegate);

  ArcIntentHelperBridge(content::BrowserContext* context,
                        ArcBridgeService* bridge_service);
  ArcIntentHelperBridge(const ArcIntentHelperBridge&) = delete;
  ArcIntentHelperBridge& operator=(const ArcIntentHelperBridge&) = delete;
  ~ArcIntentHelperBridge() override;

  // KeyedService:
  void Shutdown() override;

  // mojom::IntentHelperHost
  void OnIconInvalidated(const std::string& package_name) override;
  void OnIntentFiltersUpdated(
      std::vector<IntentFilter> intent_filters) override;
  void OnOpenDownloads() override;
  void OnOpenUrl(const std::string& url) override;
  void OnOpenCustomTab(const std::string& url,
                       int32_t task_id,
                       OnOpenCustomTabCallback callback) override;
  void OnOpenChromePage(mojom::ChromePage page) override;
  void FactoryResetArc() override;
  void OpenWallpaperPicker() override;
  void OpenVolumeControl() override;
  void OnOpenWebApp(const std::string& url) override;
  void LaunchCameraApp(uint32_t intent_id,
                       arc::mojom::CameraIntentMode mode,
                       bool should_handle_result,
                       bool should_down_scale,
                       bool is_secure,
                       int32_t task_id) override;
  void OnIntentFiltersUpdatedForPackage(
      const std::string& package_name,
      std::vector<IntentFilter> intent_filters) override;
  void CloseCameraApp() override;
  void IsChromeAppEnabled(arc::mojom::ChromeApp app,
                          IsChromeAppEnabledCallback callback) override;
  void OnSupportedLinksChanged(
      std::vector<arc::mojom::SupportedLinksPackagePtr> added_packages,
      std::vector<arc::mojom::SupportedLinksPackagePtr> removed_packages,
      arc::mojom::SupportedLinkChangeSource source) override;
  void OnDownloadAddedDeprecated(
      const std::string& relative_path,
      const std::string& owner_package_name) override;
  void OnOpenAppWithIntent(const GURL& start_url,
                           arc::mojom::LaunchIntentPtr intent) override;
  void OnOpenGlobalActions() override;
  void OnCloseSystemDialogs() override;

  // ArcIconCacheDelegete:
  GetResult GetActivityIcons(const std::vector<ActivityName>& activities,
                             OnIconsReadyCallback callback) override;

  void SetAdaptiveIconDelegate(AdaptiveIconDelegate* delegate);

  void AddObserver(ArcIntentHelperObserver* observer);
  void RemoveObserver(ArcIntentHelperObserver* observer);
  bool HasObserver(ArcIntentHelperObserver* observer) const;

  void HandleCameraResult(
      uint32_t intent_id,
      arc::mojom::CameraIntentAction action,
      const std::vector<uint8_t>& data,
      arc::mojom::IntentHelperInstance::HandleCameraResultCallback callback);

  void SendNewCaptureBroadcast(bool is_video, std::string file_path);

  void OnAndroidSettingChange(arc::mojom::AndroidSetting setting,
                              bool is_enabled) override;

  // Filters out handlers that belong to the intent_helper apk and returns
  // a new array.
  static std::vector<mojom::IntentHandlerInfoPtr> FilterOutIntentHelper(
      std::vector<mojom::IntentHandlerInfoPtr> handlers);

  const std::vector<IntentFilter>& GetIntentFilterForPackage(
      const std::string& package_name);

 private:
  THREAD_CHECKER(thread_checker_);

  const raw_ptr<content::BrowserContext> context_;
  const raw_ptr<ArcBridgeService>
      arc_bridge_service_;  // Owned by ArcServiceManager.

  ActivityIconLoader icon_loader_;

  // A map of each package name to the intent filters for that package.
  // Used to determine if Chrome should handle a URL without handing off to
  // Android.
  std::map<std::string, std::vector<IntentFilter>> intent_filters_;

  base::ObserverList<ArcIntentHelperObserver>::Unchecked observer_list_;

  // Schemes that ARC is known to send via OnOpenUrl.
  const std::set<std::string> allowed_arc_schemes_;

  std::unique_ptr<Delegate> delegate_;

  base::WeakPtrFactory<ArcIntentHelperBridge> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // COMPONENTS_ARC_INTENT_HELPER_ARC_INTENT_HELPER_BRIDGE_H_
