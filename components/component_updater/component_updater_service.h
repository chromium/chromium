// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPONENT_UPDATER_COMPONENT_UPDATER_SERVICE_H_
#define COMPONENTS_COMPONENT_UPDATER_COMPONENT_UPDATER_SERVICE_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/version.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/update_client/update_client.h"
#include "url/gurl.h"

class ComponentsHandler;
class PluginObserver;

namespace ash {
class SmartDimComponentIntegrationTest;
}

namespace policy {
class ComponentUpdaterPolicyTest;
}

namespace screen_ai {
class ScreenAIDownloaderNonChromeOS;
}

namespace speech {
class SodaInstallerImpl;
}

namespace update_client {
class Configurator;
struct CrxComponent;
struct CrxUpdateItem;
}  // namespace update_client

namespace extensions {
class AutotestPrivateLoadSmartDimComponentFunction;
}

namespace component_updater {

// Called when a non-blocking call in this module completes.
using Callback = update_client::Callback;

class OnDemandUpdater;
class UpdateScheduler;

using Configurator = update_client::Configurator;
using CrxComponent = update_client::CrxComponent;
using CrxUpdateItem = update_client::CrxUpdateItem;

struct ComponentInfo {
  ComponentInfo(const std::string& id,
                const std::string& fingerprint,
                const std::u16string& name,
                const base::Version& version,
                const std::string& cohort_id);
  ComponentInfo(const ComponentInfo& other);
  ComponentInfo& operator=(const ComponentInfo& other);
  ComponentInfo(ComponentInfo&& other);
  ComponentInfo& operator=(ComponentInfo&& other);
  ~ComponentInfo();

  std::string id;
  std::string fingerprint;
  std::u16string name;
  base::Version version;
  std::string cohort_id;
};

struct ComponentRegistration {
  ComponentRegistration(
      const std::string& app_id,
      const std::string& name,
      std::vector<uint8_t> public_key_hash,
      const base::Version& version,
      const std::string& fingerprint,
      std::map<std::string, std::string> installer_attributes,
      scoped_refptr<update_client::ActionHandler> action_handler,
      scoped_refptr<update_client::CrxInstaller> installer,
      bool requires_network_encryption,
      bool supports_group_policy_enable_component_updates,
      bool allow_cached_copies,
      bool allow_updates_on_metered_connection,
      bool allow_updates);
  ComponentRegistration(const ComponentRegistration& other);
  ComponentRegistration& operator=(const ComponentRegistration& other);
  ComponentRegistration(ComponentRegistration&& other);
  ComponentRegistration& operator=(ComponentRegistration&& other);
  ~ComponentRegistration();

  std::string app_id;
  std::string name;
  std::vector<uint8_t> public_key_hash;
  base::Version version;
  std::string fingerprint;
  std::map<std::string, std::string> installer_attributes;
  scoped_refptr<update_client::ActionHandler> action_handler;
  scoped_refptr<update_client::CrxInstaller> installer;
  bool requires_network_encryption;
  bool supports_group_policy_enable_component_updates;
  bool allow_cached_copies;
  bool allow_updates_on_metered_connection;
  bool allow_updates;
};

// The component update service is in charge of installing or upgrading select
// parts of chrome. Each part is called a component and managed by instances of
// ComponentRegistration registered using RegisterComponent(). On the
// server, each component is packaged as a CRX which is the same format used to
// package extensions. To the update service each component is identified by
// its public key hash (CrxComponent::pk_hash). If there is an update available
// and its version is bigger than (CrxComponent::version), it will be
// downloaded, verified and unpacked. Then component-specific installer
// ComponentInstaller::Install (of CrxComponent::installer) will be called.
//
// During the normal operation of the component updater some specific
// notifications are fired, like COMPONENT_UPDATER_STARTED and
// COMPONENT_UPDATE_FOUND. See notification_type.h for more details.
//
// All methods are safe to call ONLY from the browser's main sequence.
class ComponentUpdateService {
 public:
  using Observer = update_client::UpdateClient::Observer;

  // Adds an observer for this class. An observer should not be added more
  // than once. The caller retains the ownership of the observer object.
  virtual void AddObserver(Observer* observer) = 0;

  // Removes an observer. It is safe for an observer to be removed while
  // the observers are being notified.
  virtual void RemoveObserver(Observer* observer) = 0;

  // Returns the last registered version for the component associated with
  // |app_id|. Returns kNullVersion if no suitable version is found.
  virtual base::Version GetRegisteredVersion(const std::string& app_id) = 0;

  // Returns the max previous product version for the component associated with
  // |app_id|. Returns kNullVersion if no suitable version is found.
  virtual base::Version GetMaxPreviousProductVersion(
      const std::string& app_id) = 0;

  // Add component to be checked for updates.
  virtual bool RegisterComponent(const ComponentRegistration& component) = 0;

  // Unregisters the component with the given ID. This means that the component
  // is not going to be included in future update checks. If a download or
  // update operation for the component is currently in progress, it will
  // silently finish without triggering the next step.
  // Note that the installer for the component is responsible for removing any
  // existing versions of the component from disk. Returns true if the
  // uninstall has completed successfully and the component files have been
  // removed, or if the uninstalled has been deferred because the component
  // is being updated. Returns false if the component id is not known or the
  /// uninstall encountered an error.
  virtual bool UnregisterComponent(const std::string& id) = 0;

  // Returns a list of registered components.
  virtual std::vector<std::string> GetComponentIDs() const = 0;

  // Returns a list of ComponentInfo objects describing all registered
  // components.
  virtual std::vector<ComponentInfo> GetComponents() const = 0;

  // Returns an interface for on-demand updates. On-demand updates are
  // proactively triggered outside the normal component update service schedule.
  virtual OnDemandUpdater& GetOnDemandUpdater() = 0;

  // This method is used to trigger an on-demand update for component |id|.
  // This can be used when loading a resource that depends on this component.
  //
  // |callback| is called on the main sequence once the on-demand update is
  // complete, regardless of success. |callback| may be called immediately
  // within the method body.
  //
  // Additionally, this function implements an embedder-defined cooldown
  // interval between on demand update attempts. This behavior is intended
  // to be defensive against programming bugs, usually triggered by web fetches,
  // where the on-demand functionality is invoked too often. If this function
  // is called while still on cooldown, |callback| will be called immediately.
  virtual void MaybeThrottle(const std::string& id,
                             base::OnceClosure callback) = 0;

  virtual ~ComponentUpdateService() = default;

 private:
  // Returns details about registered component in the |item| parameter. The
  // function returns true in case of success and false in case of errors.
  virtual bool GetComponentDetails(const std::string& id,
                                   CrxUpdateItem* item) const = 0;

  friend class screen_ai::ScreenAIDownloaderNonChromeOS;
  friend class speech::SodaInstallerImpl;
  friend class ::ComponentsHandler;
  FRIEND_TEST_ALL_PREFIXES(ComponentInstallerTest, RegisterComponent);
  FRIEND_TEST_ALL_PREFIXES(ComponentUpdaterTest, ComponentDetails);
  FRIEND_TEST_ALL_PREFIXES(ComponentUpdaterTest, UpdatesDisabled);
};

using ServiceObserver = ComponentUpdateService::Observer;

class OnDemandUpdater {
 public:
  // The priority of the on demand update. Calls with |BACKGROUND| priority may
  // be queued up but calls with |FOREGROUND| priority may be processed right
  // away.
  enum class Priority { BACKGROUND = 0, FOREGROUND = 1 };

  virtual ~OnDemandUpdater() = default;

 private:
  friend class OnDemandTester;
  friend class policy::ComponentUpdaterPolicyTest;
  friend class ::ComponentsHandler;
  friend class ::PluginObserver;
  friend class SwReporterOnDemandFetcher;
  friend class SodaComponentInstallerPolicy;
  friend class SodaLanguagePackComponentInstallerPolicy;
  friend class ::extensions::AutotestPrivateLoadSmartDimComponentFunction;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  friend class ash::SmartDimComponentIntegrationTest;
  friend class CrOSComponentInstaller;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Triggers an update check for a component. |id| is a value
  // returned by GetCrxComponentID(). If an update for this component is already
  // in progress, the function returns |kInProgress|. If an update is available,
  // the update will be applied. The caller can subscribe to component update
  // service notifications and provide an optional callback to get the result
  // of the call. The function does not implement any cooldown interval.
  virtual void OnDemandUpdate(const std::string& id,
                              Priority priority,
                              Callback callback) = 0;
};

// Creates the component updater.
std::unique_ptr<ComponentUpdateService> ComponentUpdateServiceFactory(
    scoped_refptr<Configurator> config,
    std::unique_ptr<UpdateScheduler> scheduler,
    const std::string& brand);

// Register prefs required by the component update service.
void RegisterComponentUpdateServicePrefs(PrefRegistrySimple* registry);

}  // namespace component_updater

#endif  // COMPONENTS_COMPONENT_UPDATER_COMPONENT_UPDATER_SERVICE_H_
