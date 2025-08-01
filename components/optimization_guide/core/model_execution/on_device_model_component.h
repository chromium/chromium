// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_COMPONENT_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_COMPONENT_H_

#include <memory>
#include <string>

#include "base/containers/enum_set.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/safe_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/sequence_checker.h"
#include "base/types/pass_key.h"
#include "base/values.h"
#include "base/version.h"
#include "components/optimization_guide/core/model_execution/performance_class.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/proto/on_device_base_model_metadata.pb.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace on_device_internals {
class PageHandler;
}  // namespace on_device_internals

namespace optimization_guide {

inline constexpr std::string_view kOnDeviceModelCrxId =
    "fklghjjljmnfjoepjmlobpekiapffcja";

class OnDeviceModelComponentState;

enum class ModelBasedCapabilityKey;

// Status of the on-device model.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class OnDeviceModelStatus {
  // Model is installed and ready to use.
  kReady = 0,
  // Criteria to install model have not been met.
  kNotEligible = 1,
  // Criteria to install are met, but model installation has not completed yet.
  kInstallNotComplete = 2,
  // The model installer was not registered, even though the client would be
  // eligible to install right now. This likely means the state of the system
  // has changed recently.
  kModelInstallerNotRegisteredForUnknownReason = 3,
  // The model is ready, but it wasn't ready early enough for
  // OnDeviceModelServiceController to use it.
  kModelInstalledTooLate = 4,
  // The model is not ready, and the reason is unknown.
  kNotReadyForUnknownReason = 5,
  // Criteria (except disk space) to install are met, but the device doesn't
  // have enough disk space.
  kInsufficientDiskSpace = 6,
  // Criteria to install are met, but model is not downloaded because there was
  // no on-device feature usage.
  kNoOnDeviceFeatureUsed = 7,

  // This must be kept in sync with
  // OptimizationGuideOnDeviceModelStatus in optimization/enums.xml.

  // Insert new values before this line.
  kMaxValue = kNoOnDeviceFeatureUsed,
};

std::ostream& operator<<(std::ostream& out, OnDeviceModelStatus status);

// Wraps the specification needed to determine compatibility of the
// on-device base model with any feature specific code.
struct OnDeviceBaseModelSpec {
  using PerformanceHints =
      base::EnumSet<proto::OnDeviceModelPerformanceHint,
                    proto::OnDeviceModelPerformanceHint_MIN,
                    proto::OnDeviceModelPerformanceHint_MAX>;

  OnDeviceBaseModelSpec();
  OnDeviceBaseModelSpec(
      const std::string& model_name,
      const std::string& model_version,
      PerformanceHints supported_performance_hints);
  ~OnDeviceBaseModelSpec();
  OnDeviceBaseModelSpec(const OnDeviceBaseModelSpec&);

  bool operator==(const OnDeviceBaseModelSpec& other) const;

  // The name of the base model currently available on-device.
  std::string model_name;
  // The version of the base model currently available on-device.
  std::string model_version;
  // The supported performance hints for this device and base model.
  PerformanceHints supported_performance_hints;
};

// Manages the state of the on-device component.
// This object needs to have lifetime equal to the browser process, and outside
// of tests is created by a static NoDestructor initializer.
class OnDeviceModelComponentStateManager final {
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
        base::WeakPtr<OnDeviceModelComponentStateManager> state_manager,
        bool is_already_installing) = 0;

    // Uninstall the component. Calls
    // `OnDeviceModelComponentStateManager::UninstallComplete()` when uninstall
    // completes.
    virtual void Uninstall(
        base::WeakPtr<OnDeviceModelComponentStateManager> state_manager) = 0;
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

  struct RegistrationCriteria {
    // Requirements for install. Please update `LogInstallCriteria()` when
    // updating this.
    bool disk_space_available = false;
    bool device_capable = false;
    bool on_device_feature_recently_used = false;
    bool enabled_by_feature = false;
    bool enabled_by_enterprise_policy = false;

    // Reasons to uninstall. TODO(302327114): Add UMA for uninstall reason.
    bool running_out_of_disk_space = false;
    bool out_of_retention = false;

    // Current state.

    // We've registered the installer in the past, and haven't uninstalled yet.
    // The component may or may not be ready.
    bool is_already_installing = false;

    bool is_model_allowed() const {
      return device_capable && enabled_by_feature &&
             enabled_by_enterprise_policy;
    }

    bool should_install() const {
      if (should_uninstall()) {
        return false;
      }
      return (disk_space_available && is_model_allowed() &&
              on_device_feature_recently_used);
    }

    bool should_uninstall() const {
      return (is_already_installing &&
              (running_out_of_disk_space || out_of_retention ||
               !enabled_by_enterprise_policy));
    }
  };

  OnDeviceModelComponentStateManager(
      PrefService* local_state,
      base::SafeRef<PerformanceClassifier> performance_classifier,
      std::unique_ptr<Delegate> delegate);
  ~OnDeviceModelComponentStateManager();

  // Returns whether the component installation is valid.
  static bool VerifyInstallation(const base::FilePath& install_dir,
                                 const base::Value::Dict& manifest);

  // Called at startup. Triggers install or uninstall of the component if
  // necessary.
  void OnStartup();

  // Should be called whenever an on-device eligible feature was used.
  void OnDeviceEligibleFeatureUsed(ModelBasedCapabilityKey feature);

  // Should be called whenever the device performance class changes.
  void OnPerformanceClassAvailable();

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

  // Returns true if the installer is registered.
  bool IsInstallerRegistered();

  // Returns the current OnDeviceModelStatus.
  OnDeviceModelStatus GetOnDeviceModelStatus();

  // Exposed internal state for chrome://on-device-internals
  struct DebugState {
    int64_t disk_space_available_;
    raw_ptr<const RegistrationCriteria> criteria_;
    OnDeviceModelStatus status_;
    bool has_override_;
    raw_ptr<OnDeviceModelComponentState> state_;
  };

  // Get internal state for debugging page.
  DebugState GetDebugState(base::PassKey<on_device_internals::PageHandler>) {
    return GetDebugState();
  }

  PerformanceClassifier& performance_classifier() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return *performance_classifier_;
  }

  base::WeakPtr<OnDeviceModelComponentStateManager> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }
  base::SafeRef<OnDeviceModelComponentStateManager> GetSafeRef() {
    return weak_ptr_factory_.GetSafeRef();
  }

 private:
  enum class OnDeviceRegistrationDecision {
    // The component should be installed.
    kInstall,
    // The component should not be installed, and should be removed.
    kUninstall,
    // The component should not be installed, and does not need removed.
    kDoNotInstall,
  };

  RegistrationCriteria ComputeRegistrationCriteria(
      int64_t disk_space_free_bytes);

  DebugState GetDebugState();

  // Installs the component installer if it needs installed.
  void BeginUpdateRegistration();
  // Continuation of `UpdateRegistration()` after async work.
  void CompleteUpdateRegistration(int64_t disk_space_free_bytes);

  void OnGenAILocalFoundationalModelEnterprisePolicyChanged();

  void NotifyStateChanged();

  // Notifies the observers of the `feature` used for the first time.
  void NotifyOnDeviceEligibleFeatureFirstUsed(ModelBasedCapabilityKey feature);

  raw_ptr<PrefService> local_state_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::SafeRef<PerformanceClassifier> performance_classifier_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<Delegate> delegate_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::ObserverList<Observer> observers_ GUARDED_BY_CONTEXT(sequence_checker_);
  bool component_installer_registered_ GUARDED_BY_CONTEXT(sequence_checker_) =
      false;
  PrefChangeRegistrar pref_change_registrar_
      GUARDED_BY_CONTEXT(sequence_checker_);

  bool is_model_allowed_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  std::unique_ptr<OnDeviceModelComponentState> state_
      GUARDED_BY_CONTEXT(sequence_checker_);
  // Null until first registration attempt.
  std::unique_ptr<RegistrationCriteria> registration_criteria_
      GUARDED_BY_CONTEXT(sequence_checker_);
  // Most recently queried disk space available for model install.
  int64_t disk_space_available_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<OnDeviceModelComponentStateManager> weak_ptr_factory_{
      this};
};

// State of the on-device model component.
class OnDeviceModelComponentState {
 public:
  OnDeviceModelComponentState();
  OnDeviceModelComponentState(const OnDeviceModelComponentState&);
  ~OnDeviceModelComponentState();

  const base::FilePath& GetInstallDirectory() const { return install_dir_; }
  const base::Version& GetComponentVersion() const {
    return component_version_;
  }
  const OnDeviceBaseModelSpec& GetBaseModelSpec() const { return model_spec_; }

 private:
  friend class OnDeviceModelAdaptationLoaderTest;

  friend class OnDeviceModelComponentStateManager;

  base::FilePath install_dir_;
  base::Version component_version_;
  OnDeviceBaseModelSpec model_spec_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_COMPONENT_H_
