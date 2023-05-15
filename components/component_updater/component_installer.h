// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPONENT_UPDATER_COMPONENT_INSTALLER_H_
#define COMPONENTS_COMPONENT_UPDATER_COMPONENT_INSTALLER_H_

#include <stdint.h>

#include <memory>
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
#include "third_party/abseil-cpp/absl/types/optional.h"

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
};

// Defines the installer for Chrome components. The behavior of this class is
// controlled by an instance of ComponentInstallerPolicy, at construction time.
class ComponentInstaller final : public update_client::CrxInstaller {
 public:
  ComponentInstaller(
      std::unique_ptr<ComponentInstallerPolicy> installer_policy,
      scoped_refptr<update_client::ActionHandler> action_handler = nullptr);

  ComponentInstaller(const ComponentInstaller&) = delete;
  ComponentInstaller& operator=(const ComponentInstaller&) = delete;

  // Registers the component for update checks and installs.
  // |cus| provides the registration logic.
  // The passed |callback| will be called once the initial check for installed
  // versions is done and the component has been registered.
  // Registration tasks will be done with a priority of |task_priority|. Some
  // components may affect user-visible features, hence a default of
  // USER_VISIBLE.
  void Register(
      ComponentUpdateService* cus,
      base::OnceClosure callback,
      base::TaskPriority task_priority = base::TaskPriority::USER_VISIBLE);

  // Registers the component for update checks and installs.
  // |register_callback| is called to do the registration.
  // |callback| is called when registration finishes.
  // Registration tasks will be done with a priority of |task_priority|. Some
  // components may affect user-visible features, hence a default of
  // USER_VISIBLE.
  void Register(
      RegisterCallback register_callback,
      base::OnceClosure callback,
      base::TaskPriority task_priority = base::TaskPriority::USER_VISIBLE,
      const base::Version& registered_version = base::Version(kNullVersion));

  // Overrides from update_client::CrxInstaller.
  void OnUpdateError(int error) override;

  void Install(const base::FilePath& unpack_path,
               const std::string& public_key,
               std::unique_ptr<InstallParams> install_params,
               ProgressCallback progress_callback,
               Callback callback) override;

  bool GetInstalledFile(const std::string& file,
                        base::FilePath* installed_file) override;
  // Only user-level component installations can be uninstalled.
  bool Uninstall() override;

 private:
  struct RegistrationInfo : base::RefCountedThreadSafe<RegistrationInfo> {
    RegistrationInfo();

    RegistrationInfo(const RegistrationInfo&) = delete;
    RegistrationInfo& operator=(const RegistrationInfo&) = delete;

    base::FilePath install_dir;
    base::Version version;
    std::string fingerprint;
    absl::optional<base::Value::Dict> manifest;

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
                         scoped_refptr<RegistrationInfo> registration_info);
  void FinishRegistration(scoped_refptr<RegistrationInfo> registration_info,
                          RegisterCallback register_callback,
                          base::OnceClosure callback);
  absl::optional<base::Value::Dict> GetValidInstallationManifest(
      const base::FilePath& path);
  absl::optional<base::Version> SelectComponentVersion(
      const base::Version& registered_version,
      const base::FilePath& base_dir,
      scoped_refptr<RegistrationInfo> registration_info);

  void DeleteUnselectedComponentVersions(
      const base::FilePath& base_dir,
      const absl::optional<base::Version>& selected_version);
  absl::optional<base::FilePath> GetComponentDirectory();
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
