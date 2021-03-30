// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_INTENT_HELPER_ARC_INTENT_HELPER_BRIDGE_H_
#define COMPONENTS_ARC_INTENT_HELPER_ARC_INTENT_HELPER_BRIDGE_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "components/arc/intent_helper/activity_icon_loader.h"
#include "components/arc/intent_helper/arc_intent_helper_observer.h"
#include "components/arc/mojom/intent_helper.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/gurl.h"

class KeyedServiceBaseFactory;

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
                              public mojom::IntentHelperHost {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Resets ARC; this wipes all user data, stops ARC, then
    // re-enables ARC.
    virtual void ResetArc() = 0;
  };
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcIntentHelperBridge* GetForBrowserContext(
      content::BrowserContext* context);

  // Returns factory for the ArcIntentHelperBridge.
  static KeyedServiceBaseFactory* GetFactory();

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

  // mojom::IntentHelperHost
  void OnIconInvalidated(const std::string& package_name) override;
  void OnIntentFiltersUpdated(
      std::vector<IntentFilter> intent_filters) override;
  void OnOpenDownloads() override;
  void OnOpenUrl(const std::string& url) override;
  void OnOpenCustomTabDeprecated(const std::string& url,
                                 int32_t task_id,
                                 int32_t surface_id,
                                 int32_t top_margin,
                                 OnOpenCustomTabCallback callback) override;
  void OnOpenCustomTab(const std::string& url,
                       int32_t task_id,
                       OnOpenCustomTabCallback callback) override;
  void OnOpenChromePage(mojom::ChromePage page) override;
  void FactoryResetArc() override;
  void OpenWallpaperPicker() override;
  void SetWallpaperDeprecated(const std::vector<uint8_t>& jpeg_data) override;
  void OpenVolumeControl() override;
  void OnOpenWebApp(const std::string& url) override;
  void RecordShareFilesMetrics(mojom::ShareFiles flag) override;
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
  void OnPreferredAppsChanged(std::vector<IntentFilter> added,
                              std::vector<IntentFilter> deleted) override;

  // Retrieves icons for the |activities| and calls |callback|.
  // See ActivityIconLoader::GetActivityIcons() for more details.
  using ActivityName = internal::ActivityIconLoader::ActivityName;
  // A part of OnIconsReadyCallback signature.
  using ActivityToIconsMap = internal::ActivityIconLoader::ActivityToIconsMap;
  using OnIconsReadyCallback =
      internal::ActivityIconLoader::OnIconsReadyCallback;
  using GetResult = internal::ActivityIconLoader::GetResult;
  GetResult GetActivityIcons(const std::vector<ActivityName>& activities,
                             OnIconsReadyCallback callback);

  // Returns true when |url| can only be handled by Chrome. Otherwise, which is
  // when there might be one or more ARC apps that can handle |url|, returns
  // false. This function synchronously checks the |url| without making any IPC
  // to ARC side. Note that this function only supports http and https. If url's
  // scheme is neither http nor https, the function immediately returns true
  // without checking the filters.
  bool ShouldChromeHandleUrl(const GURL& url);

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

  // Returns false if |package_name| is for the intent_helper apk.
  static bool IsIntentHelperPackage(const std::string& package_name);

  // Filters out handlers that belong to the intent_helper apk and returns
  // a new array.
  static std::vector<mojom::IntentHandlerInfoPtr> FilterOutIntentHelper(
      std::vector<mojom::IntentHandlerInfoPtr> handlers);

  static const char kArcIntentHelperPackageName[];

  const std::vector<IntentFilter>& GetIntentFilterForPackage(
      const std::string& package_name);

  const std::vector<IntentFilter>& GetAddedPreferredApps();

  const std::vector<IntentFilter>& GetDeletedPreferredApps();

 private:
  THREAD_CHECKER(thread_checker_);

  content::BrowserContext* const context_;
  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.

  internal::ActivityIconLoader icon_loader_;

  // A map of each package name to the intent filters for that package.
  // Used to determine if Chrome should handle a URL without handing off to
  // Android.
  // TODO(crbug.com/853604): Now the package name exists in the map key as well
  // as the IntentFilter struct, it is a duplication. Should update the ARC
  // mojom type to optimise the structure.
  std::map<std::string, std::vector<IntentFilter>> intent_filters_;

  base::ObserverList<ArcIntentHelperObserver>::Unchecked observer_list_;

  // Schemes that ARC is known to send via OnOpenUrl.
  const std::set<std::string> allowed_arc_schemes_;

  // The preferred app added in ARC.
  std::vector<IntentFilter> added_preferred_apps_;

  // The preferred app deleted in ARC.
  std::vector<IntentFilter> deleted_preferred_apps_;

  std::unique_ptr<Delegate> delegate_;
};

}  // namespace arc

#endif  // COMPONENTS_ARC_INTENT_HELPER_ARC_INTENT_HELPER_BRIDGE_H_
