// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPONENT_UPDATER_COMPONENT_INSTALLER_H_
#define COMPONENTS_COMPONENT_UPDATER_COMPONENT_INSTALLER_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/task/task_traits.h"
#include "base/values.h"
#include "base/version.h"
#include "components/update_client/persisted_data.h"
#include "components/update_client/update_client.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace component_updater {

// Version "0.0.0.0" corresponds to no installed version. By the server's
// conventions, we represent it as a dotted quad.
extern const char kNullVersion[];

struct ComponentRegistration;
class ComponentUpdateService;

using RegisterCallback = base::OnceCallback<bool(const ComponentRegistration&)>;

// Components should use a ComponentInstaller by defining a class that
// implements the members of ComponentInstallerPolicy, and then registering a
// ComponentInstaller that has been constructed with an instance of that
// class.
class ComponentInstallerPolicy {
 public:
  virtual ~ComponentInstallerPolicy();

  // Verifies that a working installation resides within the directory specified
  // by |install_dir|. |install_dir| is of the form <base directory>/<version>.
  // |manifest| should have been read from the manifest file in
  // |install_dir|. Called only from a thread belonging to a blocking thread
  // pool. The implementation of this function must be efficient since the
  // function can be called when Chrome starts.
  virtual bool VerifyInstallation(const base::Value::Dict& manifest,
                                  const base::FilePath& install_dir) const = 0;

  // Returns true if the component supports a group policy to enable updates.
  // Called once during component registration from the UI thread.
  virtual bool SupportsGroupPolicyEnabledComponentUpdates() const = 0;

  // Returns true if the network communication related to this component
  // must be encrypted.
  virtual bool RequiresNetworkEncryption() const = 0;

  // OnCustomInstall is called during the installation process. Components that
  // require custom installation operations should implement them here. Returns
  // a failure result if a custom operation failed, and
  // update_client::InstallError::NONE otherwise. Called only from a thread
  // belonging to a blocking thread pool.
  virtual update_client::CrxInstaller::Result OnCustomInstall(
      const base::Value::Dict& manifest,
      const base::FilePath& install_dir) = 0;

  // OnCustomUninstall is called during the unregister (uninstall) process.
  // Components that require custom uninstallation operations should implement
  // them here.
  // Called only from a thread belonging to a blocking thread pool.
  virtual void OnCustomUninstall() = 0;

  // ComponentReady is called in two cases:
  //   1) After an installation is successfully completed.
  //   2) During component registration if the component is already installed.
  // In both cases the install is verified before this is called. This method
  // is guaranteed to be called before any observers of the component are
  // notified of a successful install, and is meant to support follow-on work
  // such as updating paths elsewhere in Chrome. Called on the UI thread.
  // |version| is the version of the component.
  // |install_dir| is the path to the install directory for this version.
  // |manifest| is the manifest for this version of the component.
  virtual void ComponentReady(const base::Version& version,
                              const base::FilePath& install_dir,
                              base::Value::Dict manifest) = 0;

  // Returns a relative path that will be appended to the component updater
  // root directories to find the data for this particular component.
  virtual base::FilePath GetRelativeInstallDir() const = 0;

  // Returns the component's SHA2 hash as raw bytes.
  virtual void GetHash(std::vector<uint8_t>* hash) const = 0;

  // Returns the human-readable name of the component.
  virtual std::string GetName() const = 0;

  // Returns a container of name-value pairs representing arbitrary,
  // installer-defined metadata.
  // The installer metadata may be used in the update checks for this component.
  // A compatible server may use these attributes to negotiate special update
  // rules when issuing an update response.
  // Valid values for the name part of an attribute match
  // ^[-_a-zA-Z0-9]{1,256}$ and valid values the value part of an attribute
  // match ^[-.,;+_=$a-zA-Z0-9]{0,256}$ .
  virtual update_client::InstallerAttributes GetInstallerAttributes() const = 0;

  // Returns true if the component allows cached copies for differential
  // updates. Defaults to |true|.
  // Differential updates are provided as patches to CRX files. Thus, an
  // unextracted CRX file is maintained by default in a cache in addition to the
  // installation of the component.
  // Existing cache entries will be removed once the component updates.
  // If the content of the component is never expected to diff well, or the
  // storage overhead is valued higher than the cost of performing a full update
  // at the expected cadence, disabling cached copies is a reasonable choice.
  virtual bool AllowCachedCopies() const;

  // Returns true if the component should not update over metered connections.
  // Defaults to |true|. This only controls whether updates are accepted: if the
  // network type changes from unmetered to metered during a download, there is
  // no guarantee that the transfer will be suspended or cancelled.
  virtual bool AllowUpdatesOnMeteredConnections() const;

  // Returns true if the component is allowed to update.
  // Defaults to |true|.
  virtual bool AllowUpdates() const;
};

// Defines the installer for Chrome components. The behavior of this class is
// controlled by an instance of ComponentInstallerPolicy, at construction time.
class ComponentInstaller final : public update_client::CrxInstaller {
 public:
  // Tasks will be done with a priority of |task_priority|. Some components may
  // affect user-visible features, hence a default of USER_VISIBLE.
  explicit ComponentInstaller(
      std::unique_ptr<ComponentInstallerPolicy> installer_policy,
      scoped_refptr<update_client::ActionHandler> action_handler = nullptr,
      base::TaskPriority task_priority = base::TaskPriority::USER_VISIBLE);

  ComponentInstaller(const ComponentInstaller&) = delete;
  ComponentInstaller& operator=(const ComponentInstaller&) = delete;

  // Registers the component for update checks and installs.
  // |cus| provides the registration logic.
  // The passed |callback| will be called once the initial check for installed
  // versions is done and the component has been registered.
  void Register(ComponentUpdateService* cus, base::OnceClosure callback);

  // Registers the component for update checks and installs.
  // |register_callback| is called to do the registration.
  // |callback| is called when registration finishes.
  void Register(
      RegisterCallback register_callback,
      base::OnceClosure callback,
      const base::Version& registered_version = base::Version(kNullVersion),
      const base::Version& max_previous_product_version =
          base::Version(kNullVersion));

  // Overrides from update_client::CrxInstaller.
  void OnUpdateError(int error) override;

  void Install(const base::FilePath& unpack_path,
               const std::string& public_key,
               std::unique_ptr<InstallParams> install_params,
               ProgressCallback progress_callback,
               Callback callback) override;

  std::optional<base::FilePath> GetInstalledFile(
      const std::string& file) override;
  // Components bundled with installations of Chrome cannot be uninstalled.
  bool Uninstall() override;

 private:
  struct RegistrationInfo : base::RefCountedThreadSafe<RegistrationInfo> {
    RegistrationInfo();

    RegistrationInfo(const RegistrationInfo&) = delete;
    RegistrationInfo& operator=(const RegistrationInfo&) = delete;

    base::FilePath install_dir;
    base::Version version;
    std::string fingerprint;
    std::optional<base::Value::Dict> manifest;

   private:
    friend class base::RefCountedThreadSafe<RegistrationInfo>;

    ~RegistrationInfo();
  };

  ~ComponentInstaller() override;

  // If there is a installation of the component set up alongside Chrome's
  // files (as opposed to in the user data directory), sets current_* to the
  // values associated with that installation and returns true; otherwise,
  // returns false.
  bool FindPreinstallation(const base::FilePath& root,
                           scoped_refptr<RegistrationInfo> registration_info);
  update_client::CrxInstaller::Result InstallHelper(
      const base::FilePath& unpack_path,
      base::Value::Dict* manifest,
      base::Version* version,
      base::FilePath* install_path);
  void StartRegistration(const base::Version& registered_version,
                         const base::Version& max_previous_product_version,
                         scoped_refptr<RegistrationInfo> registration_info);
  void FinishRegistration(scoped_refptr<RegistrationInfo> registration_info,
                          RegisterCallback register_callback,
                          base::OnceClosure callback);
  std::optional<base::Value::Dict> GetValidInstallationManifest(
      const base::FilePath& path);
  std::optional<base::Version> SelectComponentVersion(
      const base::Version& registered_version,
      const base::Version& max_previous_product_version,
      const base::FilePath& base_dir,
      scoped_refptr<RegistrationInfo> registration_info);

  void DeleteUnselectedComponentVersions(
      const base::FilePath& base_dir,
      const std::optional<base::Version>& selected_version);
  std::optional<base::FilePath> GetComponentDirectory();
  void ComponentReady(base::Value::Dict manifest);
  void UninstallOnTaskRunner();

  SEQUENCE_CHECKER(sequence_checker_);

  base::FilePath current_install_dir_;
  base::Version current_version_;
  std::string current_fingerprint_;

  std::unique_ptr<ComponentInstallerPolicy> installer_policy_;
  scoped_refptr<update_client::ActionHandler> action_handler_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  // Posts responses back to the main thread.
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;

  FRIEND_TEST_ALL_PREFIXES(ComponentInstallerTest, SelectComponentVersion);
};

}  // namespace component_updater

#endif  // COMPONENTS_COMPONENT_UPDATER_COMPONENT_INSTALLER_H_
