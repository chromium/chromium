// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_OS_INTEGRATION_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_OS_INTEGRATION_MANAGER_H_

#include <bitset>
#include <memory>
#include <vector>

#include "base/auto_reset.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/strings/string_piece_forward.h"
#include "chrome/browser/web_applications/os_integration/os_integration_sub_manager.h"
#include "chrome/browser/web_applications/os_integration/url_handler_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_file_handler_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_protocol_handler_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_run_on_os_login.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut_manager.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_registrar_observer.h"
#include "components/custom_handlers/protocol_handler.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/webapps/common/web_app_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace web_app {

class FakeOsIntegrationManager;
class WebAppProvider;

// Returns if the sub-manager architecture is enabled. This means that they are
// writing the expected os integration state to disk. See
// `AreSubManagersExecuteEnabled` to check if they are also executing.
bool AreOsIntegrationSubManagersEnabled();

// Returns if the sub-manager architecture is enabled AND the "execute"
// architecture is enabled. This causes os integration execution to happen from
// the sub-managers and not the OsIntegrationManager.
bool AreSubManagersExecuteEnabled();

// OsHooksErrors contains the result of all Os hook deployments.
// If a bit is set to `true`, then an error did occur.
using OsHooksErrors = std::bitset<OsHookType::kMaxValue + 1>;

// OsHooksOptions contains the (install/uninstall) options of all Os hook
// deployments.
using OsHooksOptions = std::bitset<OsHookType::kMaxValue + 1>;

// Used to pass install options configured from upstream caller.
// All options are disabled by default.
struct InstallOsHooksOptions {
  InstallOsHooksOptions();
  InstallOsHooksOptions(const InstallOsHooksOptions& other);
  InstallOsHooksOptions& operator=(const InstallOsHooksOptions& other);

  OsHooksOptions os_hooks;
  bool add_to_desktop = false;
  bool add_to_quick_launch_bar = false;
  ShortcutCreationReason reason = SHORTCUT_CREATION_BY_USER;
};

// Retire these 3 once the sub-manager project is done.
// Callback made after InstallOsHooks is finished.
using InstallOsHooksCallback =
    base::OnceCallback<void(OsHooksErrors os_hooks_errors)>;

// Callback made after UninstallOsHooks is finished.
using UninstallOsHooksCallback =
    base::OnceCallback<void(OsHooksErrors os_hooks_errors)>;

// Callback made after UpdateOsHooks is finished.
using UpdateOsHooksCallback =
    base::OnceCallback<void(OsHooksErrors os_hooks_errors)>;

using BarrierCallback =
    base::RepeatingCallback<void(OsHookType::Type os_hook, bool completed)>;

// OsIntegrationManager is responsible of creating/updating/deleting
// all OS hooks during Web App lifecycle.
// It contains individual OS integration managers and takes
// care of inter-dependencies among them.
class OsIntegrationManager : public WebAppRegistrarObserver {
 public:
  // Used to suppress OS hooks during this object's lifetime.
  class ScopedSuppressForTesting {
   public:
    ScopedSuppressForTesting();
    ~ScopedSuppressForTesting();

   private:
    base::AutoReset<bool> scope_;
  };

  explicit OsIntegrationManager(
      Profile* profile,
      std::unique_ptr<WebAppShortcutManager> shortcut_manager,
      std::unique_ptr<WebAppFileHandlerManager> file_handler_manager,
      std::unique_ptr<WebAppProtocolHandlerManager> protocol_handler_manager,
      std::unique_ptr<UrlHandlerManager> url_handler_manager);
  ~OsIntegrationManager() override;

  using AnyOsHooksErrorCallback =
      base::OnceCallback<void(OsHooksErrors os_hooks_errors)>;
  static base::RepeatingCallback<void(OsHooksErrors)> GetBarrierForSynchronize(
      AnyOsHooksErrorCallback errors_callback);

  // Sets internal WebAppProvider reference and threads it through to all sub
  // managers.
  virtual void SetProvider(base::PassKey<WebAppProvider>,
                           WebAppProvider& provider);

  virtual void Start();

  // Start OS Integration synchronization from external callsites. This should
  // be the only point of call into OsIntegrationManager from external places
  // after the OS integration sub managers have been implemented.
  // TODO(crbug.com/1401125): Remove all install, uninstall and update functions
  // from this file once all OS Integration sub managers have been implemented,
  // connected to the web_app system and tested.
  virtual void Synchronize(
      const webapps::AppId& app_id,
      base::OnceClosure callback,
      absl::optional<SynchronizeOsOptions> options = absl::nullopt);

  // Install all needed OS hooks for the web app.
  // If provided |web_app_info| is a nullptr, it will read icons data from disk,
  // otherwise it will use (SkBitmaps) from |web_app_info|.
  // virtual for testing
  virtual void InstallOsHooks(const webapps::AppId& app_id,
                              InstallOsHooksCallback callback,
                              std::unique_ptr<WebAppInstallInfo> web_app_info,
                              InstallOsHooksOptions options);

  // Uninstall specific OS hooks for the web app.
  // Used when removing specific hooks resulting from an app setting change.
  // Example: Running on OS login.
  // TODO(https://crbug.com/1108109) we should record uninstall result and allow
  // callback. virtual for testing
  virtual void UninstallOsHooks(const webapps::AppId& app_id,
                                const OsHooksOptions& os_hooks,
                                UninstallOsHooksCallback callback);

  // Uninstall all OS hooks for the web app.
  // Used when uninstalling a web app.
  // virtual for testing
  virtual void UninstallAllOsHooks(const webapps::AppId& app_id,
                                   UninstallOsHooksCallback callback);

  // Update all needed OS hooks for the web app.
  // virtual for testing
  virtual void UpdateOsHooks(
      const webapps::AppId& app_id,
      base::StringPiece old_name,
      FileHandlerUpdateAction file_handlers_need_os_update,
      const WebAppInstallInfo& web_app_info,
      UpdateOsHooksCallback callback);

  // Proxy calls for WebAppShortcutManager.
  // virtual for testing
  virtual void GetAppExistingShortCutLocation(
      ShortcutLocationCallback callback,
      std::unique_ptr<ShortcutInfo> shortcut_info);

  // Proxy calls for WebAppShortcutManager.
  void GetShortcutInfoForApp(
      const webapps::AppId& app_id,
      WebAppShortcutManager::GetShortcutInfoCallback callback);

  // Proxy calls for WebAppFileHandlerManager.
  bool IsFileHandlingAPIAvailable(const webapps::AppId& app_id);
  const apps::FileHandlers* GetEnabledFileHandlers(
      const webapps::AppId& app_id) const;

  // Proxy calls for WebAppProtocolHandlerManager.
  virtual absl::optional<GURL> TranslateProtocolUrl(
      const webapps::AppId& app_id,
      const GURL& protocol_url);
  virtual std::vector<custom_handlers::ProtocolHandler> GetAppProtocolHandlers(
      const webapps::AppId& app_id);
  virtual std::vector<custom_handlers::ProtocolHandler>
  GetAllowedHandlersForProtocol(const std::string& protocol);
  virtual std::vector<custom_handlers::ProtocolHandler>
  GetDisallowedHandlersForProtocol(const std::string& protocol);

  WebAppFileHandlerManager& file_handler_manager() {
    return *file_handler_manager_;
  }

  WebAppShortcutManager& shortcut_manager_for_testing();

  UrlHandlerManager& url_handler_manager_for_testing();

  WebAppProtocolHandlerManager& protocol_handler_manager_for_testing();

  virtual FakeOsIntegrationManager* AsTestOsIntegrationManager();

  void set_url_handler_manager(
      std::unique_ptr<UrlHandlerManager> url_handler_manager) {
    url_handler_manager_ = std::move(url_handler_manager);
  }

  virtual void UpdateUrlHandlers(
      const webapps::AppId& app_id,
      base::OnceCallback<void(bool success)> callback);

  virtual void UpdateFileHandlers(
      const webapps::AppId& app_id,
      FileHandlerUpdateAction file_handlers_need_os_update,
      ResultCallback finished_callback);

  // Updates protocol handler registrations with the OS.
  // If `force_shortcut_updates_if_needed` is true, then also update the
  // application's shortcuts.
  virtual void UpdateProtocolHandlers(const webapps::AppId& app_id,
                                      bool force_shortcut_updates_if_needed,
                                      base::OnceClosure callback);

  virtual void UpdateShortcuts(const webapps::AppId& app_id,
                               base::StringPiece old_name,
                               ResultCallback callback);

  // WebAppRegistrarObserver:
  void OnWebAppProfileWillBeDeleted(const webapps::AppId& app_id) override;
  void OnAppRegistrarDestroyed() override;

  void SetForceUnregisterCalledForTesting(
      base::RepeatingCallback<void(const webapps::AppId&)> on_force_unregister);

 protected:
  WebAppShortcutManager* shortcut_manager() { return shortcut_manager_.get(); }
  WebAppProtocolHandlerManager* protocol_handler_manager() {
    return protocol_handler_manager_.get();
  }
  UrlHandlerManager* url_handler_manager() {
    return url_handler_manager_.get();
  }
  void set_shortcut_manager(
      std::unique_ptr<WebAppShortcutManager> shortcut_manager) {
    shortcut_manager_ = std::move(shortcut_manager);
  }
  bool has_file_handler_manager() { return !!file_handler_manager_; }
  void set_file_handler_manager(
      std::unique_ptr<WebAppFileHandlerManager> file_handler_manager) {
    file_handler_manager_ = std::move(file_handler_manager);
  }
  void set_protocol_handler_manager(
      std::unique_ptr<WebAppProtocolHandlerManager> protocol_handler_manager) {
    protocol_handler_manager_ = std::move(protocol_handler_manager);
  }

  virtual void CreateShortcuts(const webapps::AppId& app_id,
                               bool add_to_desktop,
                               ShortcutCreationReason reason,
                               CreateShortcutsCallback callback);

  // Installation:
  virtual void RegisterFileHandlers(const webapps::AppId& app_id,
                                    ResultCallback callback);
  virtual void RegisterProtocolHandlers(const webapps::AppId& app_id,
                                        ResultCallback callback);
  virtual void RegisterUrlHandlers(const webapps::AppId& app_id,
                                   ResultCallback callback);
  virtual void RegisterShortcutsMenu(
      const webapps::AppId& app_id,
      const std::vector<WebAppShortcutsMenuItemInfo>& shortcuts_menu_item_infos,
      const ShortcutsMenuIconBitmaps& shortcuts_menu_icon_bitmaps,
      ResultCallback callback);
  virtual void ReadAllShortcutsMenuIconsAndRegisterShortcutsMenu(
      const webapps::AppId& app_id,
      ResultCallback callback);
  virtual void RegisterRunOnOsLogin(const webapps::AppId& app_id,
                                    ResultCallback callback);
  virtual void MacAppShimOnAppInstalledForProfile(const webapps::AppId& app_id);
  virtual void AddAppToQuickLaunchBar(const webapps::AppId& app_id);
  virtual void RegisterWebAppOsUninstallation(const webapps::AppId& app_id,
                                              const std::string& name);

  // Uninstallation:
  virtual bool UnregisterShortcutsMenu(const webapps::AppId& app_id,
                                       ResultCallback callback);
  virtual void UnregisterRunOnOsLogin(const webapps::AppId& app_id,
                                      ResultCallback callback);
  virtual void DeleteShortcuts(const webapps::AppId& app_id,
                               const base::FilePath& shortcuts_data_dir,
                               std::unique_ptr<ShortcutInfo> shortcut_info,
                               ResultCallback callback);
  virtual void UnregisterFileHandlers(const webapps::AppId& app_id,
                                      ResultCallback callback);
  virtual void UnregisterProtocolHandlers(const webapps::AppId& app_id,
                                          ResultCallback callback);
  virtual void UnregisterUrlHandlers(const webapps::AppId& app_id);
  virtual void UnregisterWebAppOsUninstallation(const webapps::AppId& app_id);

  // Update:
  virtual void UpdateShortcutsMenu(const webapps::AppId& app_id,
                                   const WebAppInstallInfo& web_app_info,
                                   ResultCallback callback);
  // Utility methods:
  virtual std::unique_ptr<ShortcutInfo> BuildShortcutInfo(
      const webapps::AppId& app_id);

 private:
  class OsHooksBarrier;

  // Synchronize:
  void StartSubManagerExecutionIfRequired(
      const webapps::AppId& app_id,
      absl::optional<SynchronizeOsOptions> options,
      std::unique_ptr<proto::WebAppOsIntegrationState> desired_states,
      base::OnceClosure on_all_execution_done);

  // Use to call Execute() on each sub manager recursively through callbacks
  // so as to ensure that execution happens serially in the order the sub
  // managers are stored inside the sub_managers_ vector, and that consecutive
  // sub managers execute only if the one before it has finished executing.
  void ExecuteNextSubmanager(
      const webapps::AppId& app_id,
      absl::optional<SynchronizeOsOptions> options,
      proto::WebAppOsIntegrationState* desired_state,
      const proto::WebAppOsIntegrationState current_state,
      size_t index,
      base::OnceClosure on_all_execution_done_db_write);

  void WriteStateToDB(
      const webapps::AppId& app_id,
      std::unique_ptr<proto::WebAppOsIntegrationState> desired_states,
      base::OnceClosure callback);

  // Used to call ForceUnregister() on all sub managers to remove
  // any OS integrations from the OS. This runs synchronously in the order that
  // the sub managers are stored inside the sub_managers_ vector.
  void ForceUnregisterOsIntegrationOnSubManager(
      const webapps::AppId& app_id,
      size_t index,
      base::OnceClosure final_callback);

  void OnShortcutsCreated(const webapps::AppId& app_id,
                          std::unique_ptr<WebAppInstallInfo> web_app_info,
                          InstallOsHooksOptions options,
                          scoped_refptr<OsHooksBarrier> barrier,
                          bool shortcuts_created);

  void OnShortcutsDeleted(const webapps::AppId& app_id,
                          ResultCallback callback,
                          Result result);

  void OnShortcutInfoRetrievedRegisterRunOnOsLogin(
      ResultCallback callback,
      std::unique_ptr<ShortcutInfo> info);

  // Called after the shortcuts for an app are updated in response
  // to protocol handler changes.
  // `update_finished_callback` is the callback provided in
  // `UpdateProtocolHandlers`.
  void OnShortcutsUpdatedForProtocolHandlers(
      const webapps::AppId& app_id,
      base::OnceClosure update_finished_callback);

  const raw_ptr<Profile> profile_;
  raw_ptr<WebAppProvider> provider_ = nullptr;

  std::unique_ptr<WebAppShortcutManager> shortcut_manager_;
  std::unique_ptr<WebAppFileHandlerManager> file_handler_manager_;
  std::unique_ptr<WebAppProtocolHandlerManager> protocol_handler_manager_;
  std::unique_ptr<UrlHandlerManager> url_handler_manager_;

  std::vector<std::unique_ptr<OsIntegrationSubManager>> sub_managers_;
  bool set_provider_called_ = false;
  bool first_synchronize_called_ = false;

  base::RepeatingCallback<void(const webapps::AppId&)>
      force_unregister_callback_for_testing_ = base::DoNothing();

  base::ScopedObservation<WebAppRegistrar, WebAppRegistrarObserver>
      registrar_observation_{this};

  base::WeakPtrFactory<OsIntegrationManager> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_OS_INTEGRATION_MANAGER_H_
