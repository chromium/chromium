// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_COMPONENT_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_COMPONENT_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/values.h"
#include "base/version.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"

class PrefService;

namespace optimization_guide {

inline constexpr std::string_view kOnDeviceModelCrxId =
    "fklghjjljmnfjoepjmlobpekiapffcja";

class OnDeviceModelComponentState;

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
  };

  // Creates the instance if one does not already exist. Returns an existing
  // instance otherwise.
  static scoped_refptr<OnDeviceModelComponentStateManager> CreateOrGet(
      PrefService* local_state,
      std::unique_ptr<Delegate> delegate);

  // Called at startup. Triggers install or uninstall of the component if
  // necessary.
  void OnStartup();

  // Should be called whenever an on-device eligible feature was used.
  void OnDeviceEligibleFeatureUsed();

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

  // Returns whether the component installation is valid.
  bool VerifyInstallation(const base::FilePath& install_dir,
                          const base::Value::Dict& manifest);

  // Creates the on-device component state, only called after VerifyInstallation
  // returns true.
  void SetReady(const base::Version& version,
                const base::FilePath& install_dir,
                const base::Value::Dict& manifest);

  base::WeakPtr<OnDeviceModelComponentStateManager> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // Testing functionality:
  static OnDeviceModelComponentStateManager* GetInstanceForTesting();

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

  // Called at startup to determine whether to install or uninstall the on
  // device component.
  OnDeviceRegistrationDecision GetRegistrationDecision();

  // Installs the component installer if it needs installed.
  void UpdateRegistration();

  raw_ptr<PrefService> local_state_;
  std::unique_ptr<Delegate> delegate_;
  base::ObserverList<Observer> observers_;
  bool component_installer_registered_ = false;
  std::unique_ptr<OnDeviceModelComponentState> state_;

  base::WeakPtrFactory<OnDeviceModelComponentStateManager> weak_ptr_factory_{
      this};
};

// State of the on-device model component.
class OnDeviceModelComponentState {
 public:
  ~OnDeviceModelComponentState();

  const base::FilePath& GetInstallDirectory() const { return install_dir_; }
  const base::Version& GetVersion() const { return version_; }

 private:
  OnDeviceModelComponentState();
  friend class OnDeviceModelComponentStateManager;

  base::FilePath install_dir_;
  base::Version version_;
  // Note that we'll need to read the manifest and expose additional
  // information for b/310740288.
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_COMPONENT_H_
