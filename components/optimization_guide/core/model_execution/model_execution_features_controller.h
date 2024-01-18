// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_EXECUTION_FEATURES_CONTROLLER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_EXECUTION_FEATURES_CONTROLLER_H_

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "base/threading/thread_checker.h"
#include "components/optimization_guide/core/model_execution/settings_enabled_observer.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_change_event.h"

class PrefService;

namespace optimization_guide {

// Class that keeps track of user opt-in settings, including the visibility of
// settings and the user's opt-in state.
class ModelExecutionFeaturesController
    : public signin::IdentityManager::Observer {
 public:
  enum class SettingsVisibilityResult {
    kUnknown = 0,
    // Not visible because user is not signed-in.
    kNotVisibleUnsignedUser = 1,
    // Visible because feature is already enabled.
    kVisibleFeatureAlreadyEnabled = 2,
    // Not visible because field trial is disabled.
    kNotVisibleFieldTrialDisabled = 3,
    // Visible because field trial is enabled.
    kVisibleFieldTrialEnabled = 4,
    // Not visible because feature was disabled by enterprise policy.
    kNotVisibleEnterprisePolicy = 5,
    // Not visible because model execution capability was disabled for the user
    // account.
    kNotVisibleModelExecutionCapability = 6,
    // Updates should match with FeaturesSettingsVisibilityResult enum in
    // enums.xml.
    kMaxValue = kNotVisibleModelExecutionCapability
  };

  // Must be created only for non-incognito browser contexts.
  ModelExecutionFeaturesController(PrefService* browser_context_profile_service,
                                   signin::IdentityManager* identity_manager);

  ~ModelExecutionFeaturesController() override;

  ModelExecutionFeaturesController(const ModelExecutionFeaturesController&) =
      delete;
  ModelExecutionFeaturesController& operator=(
      const ModelExecutionFeaturesController&) = delete;

  // Returns true if the opt-in setting should be shown for this profile for
  // given `feature`. This should only be called by settings UX.
  bool IsSettingVisible(proto::ModelExecutionFeature feature) const;

  // Returns true if the `feature` should be currently enabled for this user.
  // Note that the return value here may not match the feature enable state on
  // chrome settings page since the latter takes effect on browser restart.
  bool ShouldFeatureBeCurrentlyEnabledForUser(
      proto::ModelExecutionFeature feature) const;

  // Returns whether the `feature` should be currently allowed for logging model
  // quality logs.
  bool ShouldFeatureBeCurrentlyAllowedForLogging(
      proto::ModelExecutionFeature feature) const;

  // Adds `observer` which can observe the change in feature settings.
  void AddObserver(SettingsEnabledObserver* observer);

  // Removes `observer`.
  void RemoveObserver(SettingsEnabledObserver* observer);

  void SimulateBrowserRestartForTesting();

 private:
  // Enumerates the reasons an user is invalid.
  enum class UserValidityResult {
    kValid,
    kInvalidUnsignedUser,
    kInvalidEnterprisePolicy,
    kInvalidModelExecutionCapability,
  };

  // Called when the main setting toggle pref is changed.
  void OnMainToggleSettingStatePrefChanged();

  // Called when the feature-specific toggle pref is changed.
  void OnFeatureSettingPrefChanged(proto::ModelExecutionFeature feature);

  void StartObservingAccountChanges();

  // signin::IdentityManager::Observer implementation:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;
  void OnIdentityManagerShutdown(
      signin::IdentityManager* identity_manager) override;

  prefs::FeatureOptInState GetPrefState(
      proto::ModelExecutionFeature feature) const;

  // Returns the current validity result for user is eligible to be shown
  // settings for `feature`.
  UserValidityResult GetCurrentUserValidityResult(
      proto::ModelExecutionFeature feature) const;

  // Returns whether the `feature` is allowed by enterprise policy.
  bool IsAllowedByEnterprisePolicy(proto::ModelExecutionFeature feature) const;

  // Initializes the state of the different features at startup.
  void InitializeFeatureSettings();

  // Initializes pref listener to listen to changes to relevant prefs and set up
  // callbacks.
  void InitializePrefListener();

  // Resets the prefs for features that were invalid.
  void ResetInvalidFeaturePrefs();

  // Computed at the time `this` is constructed. Stores the set of features
  // that were enabled at the time when browser started.
  std::unordered_set<int> features_enabled_at_startup_;

  base::ScopedObservation<signin::IdentityManager,
                          ModelExecutionFeaturesController>
      identity_manager_observation_{this};

  raw_ptr<PrefService> browser_context_profile_service_ = nullptr;

  raw_ptr<signin::IdentityManager> identity_manager_ = nullptr;

  PrefChangeRegistrar pref_change_registrar_;

  bool is_signed_in_ = false;

  // Obtained from the user account capability. Updated whenever sign-in changes
  // or account capability changes.
  bool can_use_model_execution_features_ = false;

  base::ObserverList<SettingsEnabledObserver> observers_;

  // Set of features that are visible to unsigned users.
  const base::flat_set<proto::ModelExecutionFeature>
      features_allowed_for_unsigned_user_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_EXECUTION_FEATURES_CONTROLLER_H_
