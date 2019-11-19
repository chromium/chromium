// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_TEST_FAKE_APP_INSTANCE_H_
#define COMPONENTS_ARC_TEST_FAKE_APP_INSTANCE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "components/arc/mojom/app.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace arc {

class FakeAppInstance : public mojom::AppInstance {
 public:
  enum class IconResponseType {
    // Generate and send good icon.
    ICON_RESPONSE_SEND_GOOD,
    // Generate broken bad icon.
    ICON_RESPONSE_SEND_BAD,
    // Don't send icon.
    ICON_RESPONSE_SKIP,
  };
  class Request {
   public:
    Request(const std::string& package_name, const std::string& activity)
        : package_name_(package_name), activity_(activity) {}
    ~Request() {}

    const std::string& package_name() const { return package_name_; }

    const std::string& activity() const { return activity_; }

    bool IsForApp(const mojom::AppInfo& app_info) const {
      return package_name_ == app_info.package_name &&
             activity_ == app_info.activity;
    }

   private:
    std::string package_name_;
    std::string activity_;

    DISALLOW_COPY_AND_ASSIGN(Request);
  };

  class IconRequest : public Request {
   public:
    IconRequest(const std::string& package_name,
                const std::string& activity,
                int dimension)
        : Request(package_name, activity),
          dimension_(static_cast<int>(dimension)) {}
    ~IconRequest() {}

    int dimension() const { return dimension_; }

   private:
    const int dimension_;

    DISALLOW_COPY_AND_ASSIGN(IconRequest);
  };

  class ShortcutIconRequest {
   public:
    ShortcutIconRequest(const std::string& icon_resource_id, int dimension)
        : icon_resource_id_(icon_resource_id),
          dimension_(static_cast<int>(dimension)) {}
    ~ShortcutIconRequest() {}

    const std::string& icon_resource_id() const { return icon_resource_id_; }
    int dimension() const { return dimension_; }

   private:
    const std::string icon_resource_id_;
    const int dimension_;

    DISALLOW_COPY_AND_ASSIGN(ShortcutIconRequest);
  };

  explicit FakeAppInstance(mojom::AppHost* app_host);
  ~FakeAppInstance() override;

  // mojom::AppInstance overrides:
  void InitDeprecated(mojom::AppHostPtr host_ptr) override;
  void Init(mojom::AppHostPtr host_ptr, InitCallback callback) override;
  void LaunchAppDeprecated(const std::string& package_name,
                           const std::string& activity,
                           const base::Optional<gfx::Rect>& dimension) override;
  void LaunchApp(const std::string& package_name,
                 const std::string& activity,
                 int64_t display_id) override;
  void LaunchAppShortcutItem(const std::string& package_name,
                             const std::string& shortcut_id,
                             int64_t display_id) override;
  void RequestAppIcon(const std::string& package_name,
                      const std::string& activity,
                      int dimension,
                      RequestAppIconCallback callback) override;
  void LaunchIntentDeprecated(
      const std::string& intent_uri,
      const base::Optional<gfx::Rect>& dimension_on_screen) override;
  void LaunchIntent(const std::string& intent_uri, int64_t display_id) override;
  void RequestShortcutIcon(const std::string& icon_resource_id,
                           int dimension,
                           RequestShortcutIconCallback callback) override;
  void RequestPackageIcon(const std::string& package_name,
                          int dimension,
                          bool normalize,
                          RequestPackageIconCallback callback) override;
  void RemoveCachedIcon(const std::string& icon_resource_id) override;
  void CanHandleResolutionDeprecated(
      const std::string& package_name,
      const std::string& activity,
      const gfx::Rect& dimension,
      CanHandleResolutionDeprecatedCallback callback) override;
  void UninstallPackage(const std::string& package_name) override;
  void GetTaskInfo(int32_t task_id, GetTaskInfoCallback callback) override;
  void SetTaskActive(int32_t task_id) override;
  void CloseTask(int32_t task_id) override;
  void ShowPackageInfoDeprecated(const std::string& package_name,
                                 const gfx::Rect& dimension_on_screen) override;
  void ShowPackageInfoOnPageDeprecated(
      const std::string& package_name,
      mojom::ShowPackageInfoPage page,
      const gfx::Rect& dimension_on_screen) override;
  void ShowPackageInfoOnPage(const std::string& package_name,
                             mojom::ShowPackageInfoPage page,
                             int64_t display_id) override;
  void SetNotificationsEnabled(const std::string& package_name,
                               bool enabled) override;
  void InstallPackage(mojom::ArcPackageInfoPtr arcPackageInfo) override;

  void GetAndroidId(GetAndroidIdCallback callback) override;

  void GetRecentAndSuggestedAppsFromPlayStore(
      const std::string& query,
      int32_t max_results,
      GetRecentAndSuggestedAppsFromPlayStoreCallback callback) override;
  void GetIcingGlobalQueryResults(
      const std::string& query,
      int32_t max_results,
      GetIcingGlobalQueryResultsCallback callback) override;
  void GetAppShortcutGlobalQueryItems(
      const std::string& query,
      int32_t max_results,
      GetAppShortcutGlobalQueryItemsCallback callback) override;
  void GetAppShortcutItems(const std::string& package_name,
                           GetAppShortcutItemsCallback callback) override;

  void StartPaiFlowDeprecated() override;
  void StartPaiFlow(StartPaiFlowCallback callback) override;
  void GetAppReinstallCandidates(
      GetAppReinstallCandidatesCallback callback) override;
  void StartFastAppReinstallFlow(
      const std::vector<std::string>& package_names) override;
  void RequestAssistStructure(RequestAssistStructureCallback callback) override;
  void IsInstallable(const std::string& package_name,
                     IsInstallableCallback callback) override;

  // Methods to reply messages.
  void SendRefreshAppList(const std::vector<mojom::AppInfo>& apps);
  void SendAppAdded(const mojom::AppInfo& app);
  void SendPackageAppListRefreshed(const std::string& package_name,
                                   const std::vector<mojom::AppInfo>& apps);
  void SendTaskCreated(int32_t taskId,
                       const mojom::AppInfo& app,
                       const std::string& intent);
  void SendTaskDescription(int32_t taskId,
                           const std::string& label,
                           const std::string& icon_png_data_as_string);
  void SendTaskDestroyed(int32_t taskId);
  void SendInstallShortcut(const mojom::ShortcutInfo& shortcut);
  void SendUninstallShortcut(const std::string& package_name,
                             const std::string& intent_uri);
  void SendInstallShortcuts(const std::vector<mojom::ShortcutInfo>& shortcuts);
  void SetTaskInfo(int32_t task_id,
                   const std::string& package_name,
                   const std::string& activity);
  void SendRefreshPackageList(std::vector<mojom::ArcPackageInfoPtr> packages);
  void SendPackageAdded(mojom::ArcPackageInfoPtr package);
  void SendPackageModified(mojom::ArcPackageInfoPtr package);
  void SendPackageUninstalled(const std::string& pacakge_name);

  void SendInstallationStarted(const std::string& package_name);
  void SendInstallationFinished(const std::string& package_name,
                                bool success);

  // Returns latest icon response for particular dimension. Returns true and
  // fill |png_data_as_string| if icon for |dimension| was generated.
  bool GetIconResponse(int dimension, std::string* png_data_as_string);
  // Generates an icon for app or shorcut, determined by |app_icon| and returns:
  //   false if |icon_response_type_| is IconResponseType::ICON_RESPONSE_SKIP.
  //   true and valid png content in |png_data_as_string| if
  //        |icon_response_type_| is IconResponseType::ICON_RESPONSE_SEND_GOOD.
  //   true and invalid png content in |png_data_as_string| if
  //         |icon_response_type_| is IconResponseType::ICON_RESPONSE_SEND_BAD.
  bool GenerateIconResponse(int dimension,
                            bool app_icon,
                            std::string* png_data_as_string);

  int start_pai_request_count() const { return start_pai_request_count_; }

  int start_fast_app_reinstall_request_count() const {
    return start_fast_app_reinstall_request_count_;
  }

  void set_android_id(int64_t android_id) { android_id_ = android_id; }

  void set_icon_response_type(IconResponseType icon_response_type) {
    icon_response_type_ = icon_response_type;
  }

  void set_pai_state_response(mojom::PaiFlowState pai_state_response) {
    pai_state_response_ = pai_state_response;
  }

  int launch_app_shortcut_item_count() const {
    return launch_app_shortcut_item_count_;
  }

  const std::vector<std::unique_ptr<Request>>& launch_requests() const {
    return launch_requests_;
  }

  const std::vector<std::string>& launch_intents() const {
    return launch_intents_;
  }

  int get_app_reinstall_callback_count() const {
    return get_app_reinstall_callback_count_;
  }

  const std::vector<std::unique_ptr<IconRequest>>& icon_requests() const {
    return icon_requests_;
  }

  const std::vector<std::unique_ptr<ShortcutIconRequest>>&
  shortcut_icon_requests() const {
    return shortcut_icon_requests_;
  }

  void SetAppReinstallCandidates(
      const std::vector<arc::mojom::AppReinstallCandidatePtr>& candidates);

  void set_is_installable(bool is_installable) {
    is_installable_ = is_installable;
  }

 private:
  using TaskIdToInfo = std::map<int32_t, std::unique_ptr<Request>>;
  // Mojo endpoints.
  mojom::AppHost* app_host_;
  // Number of requests to start PAI flows.
  int start_pai_request_count_ = 0;
  // Response for PAI flow state;
  mojom::PaiFlowState pai_state_response_ = mojom::PaiFlowState::SUCCEEDED;
  // Number of requests to start Fast App Reinstall flows.
  int start_fast_app_reinstall_request_count_ = 0;
  // Keeps information about launch app shortcut requests.
  int launch_app_shortcut_item_count_ = 0;
  // Keeps info about the number of times we got a request for app reinstalls.
  int get_app_reinstall_callback_count_ = 0;
  // AndroidId to return.
  int64_t android_id_ = 0;

  // Vector to send as app reinstall candidates.
  std::vector<arc::mojom::AppReinstallCandidatePtr> app_reinstall_candidates_;
  // Keeps information about launch requests.
  std::vector<std::unique_ptr<Request>> launch_requests_;
  // Keeps information about launch intents.
  std::vector<std::string> launch_intents_;
  // Keeps information about icon load requests.
  std::vector<std::unique_ptr<IconRequest>> icon_requests_;
  // Keeps information about shortcut icon load requests.
  std::vector<std::unique_ptr<ShortcutIconRequest>> shortcut_icon_requests_;
  // Keeps information for running tasks.
  TaskIdToInfo task_id_to_info_;
  // Defines how to response to icon requests.
  IconResponseType icon_response_type_ =
      IconResponseType::ICON_RESPONSE_SEND_GOOD;
  // Keeps latest generated icons per icon dimension.
  std::map<int, std::string> icon_responses_;

  bool is_installable_ = false;

  // Keeps the binding alive so that calls to this class can be correctly
  // routed.
  mojom::AppHostPtr host_;

  bool GetFakeIcon(mojom::ScaleFactor scale_factor,
                   std::string* png_data_as_string);

  DISALLOW_COPY_AND_ASSIGN(FakeAppInstance);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_TEST_FAKE_APP_INSTANCE_H_
