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
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/web_applications/os_integration/os_integration_sub_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_file_handler_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_protocol_handler_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_run_on_os_login.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "components/custom_handlers/protocol_handler.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/webapps/common/web_app_id.h"

class ProfileManager;
class Profile;
class ScopedProfileKeepAlive;

namespace web_app {

class FakeOsIntegrationManager;
class WebAppProvider;

using ShortcutLocationCallback =
    base::OnceCallback<void(ShortcutLocations shortcut_locations)>;

// Returns the ShortcutInfo for an app.
using GetShortcutInfoCallback =
    base::OnceCallback<void(std::unique_ptr<ShortcutInfo>)>;

// OsIntegrationManager is responsible of creating/updating/deleting
// all OS hooks during Web App lifecycle.
// It contains individual OS integration managers and takes
// care of inter-dependencies among them.
class OsIntegrationManager : public ProfileManagerObserver {
 public:
  using UpdateShortcutsForAllAppsCallback =
      base::RepeatingCallback<void(Profile*, base::OnceClosure)>;

  // Used to suppress OS hooks during this object's lifetime.
  class ScopedSuppressForTesting {
   public:
    ScopedSuppressForTesting();
    ~ScopedSuppressForTesting();
  };
  static bool AreOsHooksSuppressedForTesting();

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Sets a callback to be called when this class determines that all shortcuts
  // for a particular profile need to be rebuild, for example because the app
  // shortcut version has changed since the last time these were created.
  // This is used by the legacy extensions based app code in
  // chrome/browser/web_applications/extensions to ensure those app shortcuts
  // also get updated. Calling out to that code directly would violate
  // dependency layering.
  static void SetUpdateShortcutsForAllAppsCallback(
      UpdateShortcutsForAllAppsCallback callback);

  static base::OnceClosure& OnSetCurrentAppShortcutsVersionCallbackForTesting();

  explicit OsIntegrationManager(
      Profile* profile,
      std::unique_ptr<WebAppFileHandlerManager> file_handler_manager,
      std::unique_ptr<WebAppProtocolHandlerManager> protocol_handler_manager);
  ~OsIntegrationManager() override;

  // Sets internal WebAppProvider reference and threads it through to all sub
  // managers.
  virtual void SetProvider(base::PassKey<WebAppProvider>,
                           WebAppProvider& provider);

  virtual void Start();
  void Shutdown();

  // Start OS Integration synchronization from external callsites. This should
  // be the only point of call into OsIntegrationManager from external places
  // after the OS integration sub managers have been implemented.
  virtual void Synchronize(
      const webapps::AppId& app_id,
      base::OnceClosure callback,
      std::optional<SynchronizeOsOptions> options = std::nullopt);

  // Asynchronously gathers existing shortcut locations according to
  // `shortcut_info`, the results of which will be passed into `callback`.
  // Virtual for testing.
  virtual void GetAppExistingShortCutLocation(
      ShortcutLocationCallback callback,
      std::unique_ptr<ShortcutInfo> shortcut_info);

  // Asynchronously gets the information required to create a shortcut for
  // `app_id` from the WebAppRegistrar along with the icon bitmaps. Do note that
  // this information is obtained from fields other than the web app's
  // `current_os_integration_state_` field, so there is still a slight chance
  // that the information returned from the registrar might not match the web
  // app's current OS integration state (for example if this API is triggered in
  // between the registrar being updated and OS integration being completed).
  //
  // If ShortcutInfo creation requires using the current OS integration state of
  // the web_app, prefer calling `web_app::BuildShortcutInfoWithoutFavicon()`
  // instead.
  virtual void GetShortcutInfoForAppFromRegistrar(
      const webapps::AppId& app_id,
      GetShortcutInfoCallback callback);

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

  void SetForceUnregisterCalledForTesting(
      base::RepeatingCallback<void(const webapps::AppId&)> on_force_unregister);

  // ProfileManagerObserver:
  void OnProfileMarkedForPermanentDeletion(
      Profile* profile_to_be_deleted) override;
  void OnProfileManagerDestroying() override;

 protected:
  WebAppProtocolHandlerManager* protocol_handler_manager() {
    return protocol_handler_manager_.get();
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

  // If a profile is marked for deletion, remove all OS integration for an app
  // installed for that profile.
  void UnregisterOsIntegrationOnProfileMarkedForDeletion(
      const webapps::AppId& app_id);

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

  // Schedules a call to UpdateShortcutsForAllAppsNow() if kAppShortcutsVersion
  // in prefs is less than kCurrentAppShortcutsVersion.
  void UpdateShortcutsForAllAppsIfNeeded();
  void UpdateShortcutsForAllAppsNow();
  void SetCurrentAppShortcutsVersion();

  void OnIconsRead(const webapps::AppId& app_id,
                   GetShortcutInfoCallback callback,
                   std::map<SquareSizePx, SkBitmap> icon_bitmaps);

  std::unique_ptr<ShortcutInfo> BuildShortcutInfoForWebApp(const WebApp* app);

  const raw_ptr<Profile> profile_;
  raw_ptr<WebAppProvider> provider_ = nullptr;

  std::unique_ptr<WebAppFileHandlerManager> file_handler_manager_;
  std::unique_ptr<WebAppProtocolHandlerManager> protocol_handler_manager_;

  std::vector<std::unique_ptr<OsIntegrationSubManager>> sub_managers_;
  bool set_provider_called_ = false;
  bool first_synchronize_called_ = false;

  base::RepeatingCallback<void(const webapps::AppId&)>
      force_unregister_callback_for_testing_ = base::DoNothing();

  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observation_{this};

  base::WeakPtrFactory<OsIntegrationManager> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_OS_INTEGRATION_MANAGER_H_
