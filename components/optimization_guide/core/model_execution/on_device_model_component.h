// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_COMPONENT_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_COMPONENT_H_

#include <memory>
#include <optional>
#include <string>

#include "base/byte_count.h"
#include "base/containers/enum_set.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/safe_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/types/expected.h"
#include "base/types/pass_key.h"
#include "base/values.h"
#include "base/version.h"
#include "components/optimization_guide/core/model_execution/performance_class.h"
#include "components/optimization_guide/core/model_execution/usage_tracker.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/on_device_base_model_metadata.pb.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-forward.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace on_device_internals {
class PageHandler;
}  // namespace on_device_internals

namespace optimization_guide {

inline constexpr std::string_view kOnDeviceModelCrxId =
    "fklghjjljmnfjoepjmlobpekiapffcja";

class UsageTracker;

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
  // OnDeviceModelServiceController to use it. (Not used anymore, keep it for
  // logs in the past).
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

// Identifies a specific on-device base model and the performance hint that
// it will be used with.
struct OnDeviceBaseModelSpec {
  using PerformanceHints =
      base::EnumSet<proto::OnDeviceModelPerformanceHint,
                    proto::OnDeviceModelPerformanceHint_MIN,
                    proto::OnDeviceModelPerformanceHint_MAX>;

  OnDeviceBaseModelSpec(
      const std::string& model_name,
      const std::string& model_version,
      proto::OnDeviceModelPerformanceHint selected_performance_hint);
  ~OnDeviceBaseModelSpec();
  OnDeviceBaseModelSpec(const OnDeviceBaseModelSpec&);

  bool operator==(const OnDeviceBaseModelSpec& other) const;

  // The name of the base model currently available on-device.
  std::string model_name;
  // The version of the base model currently available on-device.
  std::string model_version;
  // The selected performance hint for this device and base model.
  proto::OnDeviceModelPerformanceHint selected_performance_hint;
};

// State of the on-device model component.
class OnDeviceModelComponentState {
 public:
  OnDeviceModelComponentState(base::FilePath install_dir,
                              base::Version component_version,
                              OnDeviceBaseModelSpec model_spec);
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

enum class ModelInstallMode {
  // Install the model on-demand (foreground download).
  kOnDemand = 0,
  // Install the model on regular schedule (background download).
  kBackground = 1,
  kMaxValue = kBackground,
};

// The attributes selected when registering an on-device model component.
struct OnDeviceModelRegistrationAttributes {
 public:
  using Hint = optimization_guide::proto::OnDeviceModelPerformanceHint;

  explicit OnDeviceModelRegistrationAttributes(
      base::flat_set<Hint> supported_hints);
  OnDeviceModelRegistrationAttributes(
      const OnDeviceModelRegistrationAttributes&);
  OnDeviceModelRegistrationAttributes& operator=(
      const OnDeviceModelRegistrationAttributes&);
  OnDeviceModelRegistrationAttributes(OnDeviceModelRegistrationAttributes&&);
  OnDeviceModelRegistrationAttributes& operator=(
      OnDeviceModelRegistrationAttributes&&);
  ~OnDeviceModelRegistrationAttributes();
  // The performance hints that are supported by this device.
  base::flat_set<Hint> supported_hints;
};

using MaybeOnDeviceModelComponentState =
    base::expected<std::reference_wrapper<const OnDeviceModelComponentState>,
                   OnDeviceModelStatus>;

// Manages the state of the on-device component.
// This object needs to have lifetime equal to the browser process, and outside
// of tests is created by a static NoDestructor initializer.
class OnDeviceModelComponentStateManager final : public UsageTracker::Observer {
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
        base::OnceCallback<void(std::optional<base::ByteCount>)> callback) = 0;

    // Registers the component installer. Calls
    // `OnDeviceModelComponentStateManager::SetReady` when the component is
    // ready to use.
    virtual void RegisterInstaller(
        base::WeakPtr<OnDeviceModelComponentStateManager> state_manager,
        OnDeviceModelRegistrationAttributes attributes) = 0;

    // Uninstall the component. Calls
    // `OnDeviceModelComponentStateManager::UninstallComplete()` when uninstall
    // completes.
    virtual void Uninstall(
        base::WeakPtr<OnDeviceModelComponentStateManager> state_manager) = 0;

    // Request on demand update. Assumes that `RegisterInstaller` has already
    // been called.
    virtual void RequestUpdate(bool is_background) = 0;

    // Gets the base model component ID.
    virtual std::string GetComponentId() = 0;
  };

  class Observer : public base::CheckedObserver {
   public:
    // Called whenever the on-device component state changes. `state` is null if
    // the component is not available.
    virtual void StateChanged(MaybeOnDeviceModelComponentState state) = 0;
  };

  struct RegistrationCriteria {
    RegistrationCriteria();
    ~RegistrationCriteria();
    RegistrationCriteria(const RegistrationCriteria&);
    RegistrationCriteria& operator=(const RegistrationCriteria&);
    RegistrationCriteria(RegistrationCriteria&&);
    RegistrationCriteria& operator=(RegistrationCriteria&&);

    // Requirements for install. Please update `LogInstallCriteria()` when
    // updating this.
    bool device_capable = false;
    bool on_device_feature_recently_used = false;
    bool enabled_by_feature = false;
    bool enabled_by_enterprise_policy = false;
    bool enabled_by_user_setting = false;
    // Criteria for background download.
    bool is_on_external_power = false;

    // Reasons to uninstall. TODO(302327114): Add UMA for uninstall reason.
    bool out_of_retention = false;

    // Current state.

    // We've registered the installer in the past, and haven't uninstalled yet.
    // The component may or may not be ready.
    bool is_already_installing = false;

    // Whether background download was requested.
    bool background_download_requested = false;

    // Most recently queried disk space available for model install.
    base::ByteCount disk_space_free;

    bool is_disk_space_available() const {
      return features::IsFreeDiskSpaceSufficientForOnDeviceModelInstall(
          disk_space_free);
    }

    bool is_running_out_of_disk_space() const {
      return features::IsFreeDiskSpaceTooLowForOnDeviceModelInstall(
          disk_space_free);
    }

    bool is_model_allowed() const {
      return device_capable && enabled_by_feature &&
             enabled_by_enterprise_policy && enabled_by_user_setting;
    }

    std::optional<ModelInstallMode> get_install_mode() const {
      if (should_uninstall() || !is_disk_space_available() ||
          !is_model_allowed()) {
        return std::nullopt;
      }
      if (on_device_feature_recently_used) {
        return ModelInstallMode::kOnDemand;
      }
      if (background_download_requested &&
          base::FeatureList::IsEnabled(
              features::kOnDeviceModelBackgroundDownload)) {
        if (is_on_external_power &&
            features::
                IsFreeDiskSpaceSufficientForBackgroundOnDeviceModelInstall(
                    disk_space_free)) {
          return ModelInstallMode::kBackground;
        }
      }
      return std::nullopt;
    }

    bool should_uninstall() const {
      return (is_already_installing &&
              (is_running_out_of_disk_space() || out_of_retention ||
               !enabled_by_enterprise_policy || !enabled_by_user_setting));
    }
  };

  enum class ComponentInstallerState {
    // Component not registered, e.g, already uninstalled, never installed.
    kNotRegistered,
    // RegisterInstaller called, waiting for completion.
    kRegistering,
    // Registration completed, installation may or may not be happening yet.
    kRegistered,
    // Registered and requested on demand update with background priority.
    kBackgroundDownloading,
    // Registered and requested on demand update with foreground priority.
    kOnDemandDownloading,
    // Component is fully installed.
    kInstalled,
    // Uninstall called, waiting for completion.
    kUninstalling,
  };

  OnDeviceModelComponentStateManager(
      PrefService* local_state,
      base::SafeRef<PerformanceClassifier> performance_classifier,
      UsageTracker& usage_tracker,
      std::unique_ptr<Delegate> delegate);
  ~OnDeviceModelComponentStateManager() override;

  // Returns whether the component installation is valid.
  static bool VerifyInstallation(const base::FilePath& install_dir,
                                 const base::DictValue& manifest);

  // Returns the current state. Null if the component is not available.
  const OnDeviceModelComponentState* GetState();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Exposed internal state for chrome://on-device-internals
  struct DebugState {
    base::ByteCount disk_space_available_;
    raw_ptr<const RegistrationCriteria> criteria_;
    OnDeviceModelStatus status_;
    bool has_override_;
    raw_ptr<OnDeviceModelComponentState> state_;
  };

  // Get internal state for debugging page.
  DebugState GetDebugState(base::PassKey<on_device_internals::PageHandler>) {
    return GetDebugState();
  }

  // Functions called by the component installer:

  // Creates the on-device component state, only called after VerifyInstallation
  // returns true.
  void SetReady(const base::Version& version,
                const base::FilePath& install_dir,
                const base::DictValue& manifest);

  // Called after the installer is successfully registered.
  void InstallerRegistered(bool is_already_installed);

  // Called when the on-device component has been uninstalled.
  void UninstallComplete();

  // Used by the chrome://on-device-internals page to uninstall the model.
  void ForceUninstall();

  // Starts the background model download. No-op if the component is already
  // installed. Used for proactively downloading the model.
  void MaybeBeginBackgroundModelDownload();

  base::WeakPtr<OnDeviceModelComponentStateManager> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  DebugState GetDebugState();

  // Should be called whenever the device performance class changes.
  void OnPerformanceClassAvailable();

  void OnGenAILocalFoundationalModelEnterprisePolicyChanged();

  void OnGenAILocalFoundationalModelUserSettingChanged();

  // UsageTracker::Observer:
  void OnDeviceEligibleFeatureUsed(mojom::OnDeviceFeature feature) override;

  // Installs the component installer if it needs installed.
  void BeginUpdateRegistration();
  RegistrationCriteria ComputeRegistrationCriteria(
      base::ByteCount disk_space_free_bytes);
  // Continuation of `UpdateRegistration()` after async work.
  void CompleteUpdateRegistration(
      std::optional<base::ByteCount> disk_space_free);

  void UpdateRegistrationCriteria(
      std::optional<base::ByteCount> disk_space_free);
  void UpdateRegistration();

  // Uninstalls the component.
  void UninstallComponent();

  void NotifyStateChanged();

  MaybeOnDeviceModelComponentState GetOnDeviceModelState();

  // Returns the current OnDeviceModelStatus.
  OnDeviceModelStatus GetOnDeviceModelStatus();

  raw_ptr<PrefService> local_state_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::SafeRef<PerformanceClassifier> performance_classifier_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<Delegate> delegate_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::ObserverList<Observer> observers_ GUARDED_BY_CONTEXT(sequence_checker_);
  ComponentInstallerState component_installer_state_ GUARDED_BY_CONTEXT(
      sequence_checker_) = ComponentInstallerState::kNotRegistered;
  bool background_download_requested_ GUARDED_BY_CONTEXT(sequence_checker_) =
      false;
  PrefChangeRegistrar pref_change_registrar_
      GUARDED_BY_CONTEXT(sequence_checker_);

  std::unique_ptr<OnDeviceModelComponentState> state_
      GUARDED_BY_CONTEXT(sequence_checker_);
  // Null until first registration attempt.
  std::unique_ptr<RegistrationCriteria> registration_criteria_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::raw_ref<UsageTracker> usage_tracker_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::ScopedObservation<UsageTracker, UsageTracker::Observer>
      usage_tracker_observation_{this};

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<OnDeviceModelComponentStateManager> weak_ptr_factory_{
      this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_COMPONENT_H_
