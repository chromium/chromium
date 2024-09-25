// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_COMPONENT_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_COMPONENT_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/sequence_checker.h"
#include "base/values.h"
#include "base/version.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"

class PrefService;

namespace optimization_guide {

inline constexpr std::string_view kOnDeviceModelCrxId =
    "fklghjjljmnfjoepjmlobpekiapffcja";

class OnDeviceModelComponentState;

enum class ModelBasedCapabilityKey;

// Wraps the specification needed to determine compatibility of the
// on-device base model with any feature specific code.
struct OnDeviceBaseModelSpec {
  // The name of the base model currently available on-device.
  std::string model_name;
  // The version of the base model currently available on-device.
  std::string model_version;
  // Note that we may need to read the manifest and expose additional
  // information for b/310740288 beyond the name and version in the future.
};

// Manages the state of the on-device component.
// This object needs to have lifetime equal to the browser process. This is
// achieved by holding a scoped_refptr on KeyedServices which need it, and on
// the installer (which is owned by ComponentUpdaterService).
class OnDeviceModelComponentStateManager
    : public base::RefCounted<OnDeviceModelComponentStateManager> {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Returns the directory where the component would be installed.
    virtual base::FilePath GetInstallDirectory() = 0;

    // Calls `base::SysInfo::AmountOfFreeDiskSpace()` on a background sequence,
    // and calls `callback`.
    virtual void GetFreeDiskSpace(
        const base::FilePath& path,
        base::OnceCallback<void(int64_t)> callback) = 0;

    // Registers the component installer. Calls
    // `OnDeviceModelComponentStateManager::SetReady` when the component is
    // ready to use.
    virtual void RegisterInstaller(
        scoped_refptr<OnDeviceModelComponentStateManager> state_manager) = 0;

    // Uninstall the component. Calls
    // `OnDeviceModelComponentStateManager::UninstallComplete()` when uninstall
    // completes.
    virtual void Uninstall(
        scoped_refptr<OnDeviceModelComponentStateManager> state_manager) = 0;
  };

  class Observer : public base::CheckedObserver {
   public:
    // Called whenever the on-device component state changes. `state` is null if
    // the component is not available.
    virtual void StateChanged(const OnDeviceModelComponentState* state) = 0;

    // Called when on-device eligible `feature` was used for the first time.
    // This is called when at startup the feature was not used, and then gets
    // used for the first time.
    virtual void OnDeviceEligibleFeatureFirstUsed(
        ModelBasedCapabilityKey feature) {}
  };

  // Creates the instance if one does not already exist. Returns an existing
  // instance otherwise.
  static scoped_refptr<OnDeviceModelComponentStateManager> CreateOrGet(
      PrefService* local_state,
      std::unique_ptr<Delegate> delegate);

  // Returns whether the component installation is valid.
  static bool VerifyInstallation(const base::FilePath& install_dir,
                                 const base::Value::Dict& manifest);

  // Called at startup. Triggers install or uninstall of the component if
  // necessary.
  void OnStartup();

  // Should be called whenever an on-device eligible feature was used.
  void OnDeviceEligibleFeatureUsed(ModelBasedCapabilityKey feature);

  // Should be called whenever the device performance class changes.
  void DevicePerformanceClassChanged(
      OnDeviceModelPerformanceClass performance_class);

  // Returns the current state. Null if the component is not available.
  const OnDeviceModelComponentState* GetState();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Functions called by the component installer:

  // Called when the on-device component has been uninstalled.
  void UninstallComplete();

  // Creates the on-device component state, only called after VerifyInstallation
  // returns true.
  void SetReady(const base::Version& version,
                const base::FilePath& install_dir,
                const base::Value::Dict& manifest);

  // Called after the installer is successfully registered.
  void InstallerRegistered();

  // Returns the current OnDeviceModelStatus.
  OnDeviceModelStatus GetOnDeviceModelStatus();

  base::WeakPtr<OnDeviceModelComponentStateManager> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // Testing functionality:
  static OnDeviceModelComponentStateManager* GetInstanceForTesting();

  struct RegistrationCriteria;

 private:
  friend class base::RefCounted<OnDeviceModelComponentStateManager>;

  enum class OnDeviceRegistrationDecision {
    // The component should be installed.
    kInstall,
    // The component should not be installed, and should be removed.
    kUninstall,
    // The component should not be installed, and does not need removed.
    kDoNotInstall,
  };

  OnDeviceModelComponentStateManager(PrefService* local_state,
                                     std::unique_ptr<Delegate> delegate);
  ~OnDeviceModelComponentStateManager();

  RegistrationCriteria GetRegistrationCriteria(int64_t disk_space_free_bytes);

  // Installs the component installer if it needs installed.
  void BeginUpdateRegistration();
  // Continuation of `UpdateRegistration()` after async work.
  void CompleteUpdateRegistration(int64_t disk_space_free_bytes);

  void NotifyStateChanged();

  // Notifies the observers of the `feature` used for the first time.
  void NotifyOnDeviceEligibleFeatureFirstUsed(ModelBasedCapabilityKey feature);

  raw_ptr<PrefService> local_state_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<Delegate> delegate_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::ObserverList<Observer> observers_ GUARDED_BY_CONTEXT(sequence_checker_);
  bool component_installer_registered_ GUARDED_BY_CONTEXT(sequence_checker_) =
      false;

  bool is_model_allowed_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  std::unique_ptr<OnDeviceModelComponentState> state_
      GUARDED_BY_CONTEXT(sequence_checker_);
  // Null until first registration attempt.
  std::unique_ptr<RegistrationCriteria> registration_criteria_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<OnDeviceModelComponentStateManager> weak_ptr_factory_{
      this};
};

// State of the on-device model component.
class OnDeviceModelComponentState {
 public:
  ~OnDeviceModelComponentState();

  const base::FilePath& GetInstallDirectory() const { return install_dir_; }
  const base::Version& GetComponentVersion() const {
    return component_version_;
  }
  const OnDeviceBaseModelSpec& GetBaseModelSpec() const { return model_spec_; }

 private:
  friend class OnDeviceModelAdaptationLoaderTest;

  OnDeviceModelComponentState();
  friend class OnDeviceModelComponentStateManager;

  base::FilePath install_dir_;
  base::Version component_version_;
  OnDeviceBaseModelSpec model_spec_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_COMPONENT_H_
