// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_OS_INTEGRATION_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_OS_INTEGRATION_MANAGER_H_

#include <bitset>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/web_applications/os_integration/os_integration_sub_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_file_handler_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_protocol_handler_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_run_on_os_login.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut_manager.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registrar_observer.h"
#include "components/custom_handlers/protocol_handler.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/webapps/common/web_app_id.h"

class Profile;
class ScopedProfileKeepAlive;

namespace web_app {

class FakeOsIntegrationManager;
class WebAppProvider;

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
  };
  static bool AreOsHooksSuppressedForTesting();

  explicit OsIntegrationManager(
      Profile* profile,
      std::unique_ptr<WebAppShortcutManager> shortcut_manager,
      std::unique_ptr<WebAppFileHandlerManager> file_handler_manager,
      std::unique_ptr<WebAppProtocolHandlerManager> protocol_handler_manager);
  ~OsIntegrationManager() override;

  // Sets internal WebAppProvider reference and threads it through to all sub
  // managers.
  virtual void SetProvider(base::PassKey<WebAppProvider>,
                           WebAppProvider& provider);

  virtual void Start();

  // Start OS Integration synchronization from external callsites. This should
  // be the only point of call into OsIntegrationManager from external places
  // after the OS integration sub managers have been implemented.
  // TODO(crbug.com/40250591): Remove all install, uninstall and update
  // functions from this file once all OS Integration sub managers have been
  // implemented, connected to the web_app system and tested.
  virtual void Synchronize(
      const webapps::AppId& app_id,
      base::OnceClosure callback,
      std::optional<SynchronizeOsOptions> options = std::nullopt);

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
  virtual std::optional<GURL> TranslateProtocolUrl(const webapps::AppId& app_id,
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

  WebAppProtocolHandlerManager& protocol_handler_manager_for_testing();

  virtual FakeOsIntegrationManager* AsTestOsIntegrationManager();

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

 private:
  // Synchronize:
  void StartSubManagerExecutionIfRequired(
      const webapps::AppId& app_id,
      std::optional<SynchronizeOsOptions> options,
      std::unique_ptr<proto::WebAppOsIntegrationState> desired_states,
      base::OnceClosure on_all_execution_done);

  // Use to call Execute() on each sub manager recursively through callbacks
  // so as to ensure that execution happens serially in the order the sub
  // managers are stored inside the sub_managers_ vector, and that consecutive
  // sub managers execute only if the one before it has finished executing.
  void ExecuteNextSubmanager(
      const webapps::AppId& app_id,
      std::optional<SynchronizeOsOptions> options,
      proto::WebAppOsIntegrationState* desired_state,
      const proto::WebAppOsIntegrationState current_state,
      size_t index,
      base::OnceClosure on_all_execution_done_db_write);

  void WriteStateToDB(
      const webapps::AppId& app_id,
      std::unique_ptr<proto::WebAppOsIntegrationState> desired_states,
      base::OnceClosure callback);

  // Called when ForceUnregisterOsIntegrationSubManager has finished
  // unregistering sub managers. `keep_alive` is reset to allow the
  // profile to be deleted.
  void SubManagersUnregistered(
      const webapps::AppId& app_id,
      std::unique_ptr<ScopedProfileKeepAlive> keep_alive);

  // Used to call ForceUnregister() on all sub managers to remove
  // any OS integrations from the OS. This runs synchronously in the order that
  // the sub managers are stored inside the sub_managers_ vector.
  void ForceUnregisterOsIntegrationOnSubManager(
      const webapps::AppId& app_id,
      size_t index,
      base::OnceClosure final_callback);

  const raw_ptr<Profile> profile_;
  raw_ptr<WebAppProvider> provider_ = nullptr;

  std::unique_ptr<WebAppShortcutManager> shortcut_manager_;
  std::unique_ptr<WebAppFileHandlerManager> file_handler_manager_;
  std::unique_ptr<WebAppProtocolHandlerManager> protocol_handler_manager_;

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
