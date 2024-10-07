// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_EXECUTION_FEATURES_CONTROLLER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_EXECUTION_FEATURES_CONTROLLER_H_

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "base/threading/thread_checker.h"
#include "components/optimization_guide/core/feature_registry/mqls_feature_registry.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
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
    // Not visible because the feature is already graduated.
    kNotVisibleGraduatedFeature = 7,
    // Not visible because the device is unsupported by the feature.
    kNotVisibleHardwareUnsupported = 8,
    // Updates should match with FeaturesSettingsVisibilityResult enum in
    // enums.xml.
    kMaxValue = kNotVisibleHardwareUnsupported
  };

  enum class DogfoodStatus {
    DOGFOOD,
    NON_DOGFOOD,
  };

  // Must be created only for non-incognito browser contexts.
  ModelExecutionFeaturesController(PrefService* browser_context_profile_service,
                                   signin::IdentityManager* identity_manager,
                                   PrefService* local_state,
                                   DogfoodStatus dogfood_status);

  ~ModelExecutionFeaturesController() override;

  ModelExecutionFeaturesController(const ModelExecutionFeaturesController&) =
      delete;
  ModelExecutionFeaturesController& operator=(
      const ModelExecutionFeaturesController&) = delete;

  // Returns true if the opt-in setting should be shown for this profile for
  // given `feature`. This should only be called by settings UX.
  bool IsSettingVisible(UserVisibleFeatureKey feature) const;

  // Returns true if the `feature` should be currently enabled for this user.
  // Note that the return value here may not match the feature enable state on
  // chrome settings page since the latter takes effect on browser restart.
  bool ShouldFeatureBeCurrentlyEnabledForUser(
      UserVisibleFeatureKey feature) const;

  // Returns true if signed-in user is allowed to execute models, disregarding
  // the `allow_unsigned_user` switch.
  bool ShouldFeatureAllowModelExecutionForSignedInUser(
      optimization_guide::UserVisibleFeatureKey feature) const;

  // Returns whether the `feature` should be currently allowed for logging model
  // quality logs.
  bool ShouldFeatureBeCurrentlyAllowedForLogging(
      const MqlsFeatureMetadata* metadata) const;

  // Adds `observer` which can observe the change in feature settings.
  void AddObserver(SettingsEnabledObserver* observer);

  // Removes `observer`.
  void RemoveObserver(SettingsEnabledObserver* observer);

  base::WeakPtr<ModelExecutionFeaturesController> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void AllowUnsignedUserForTesting(UserVisibleFeatureKey feature);

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
  void OnFeatureSettingPrefChanged(UserVisibleFeatureKey feature);

  // Called when the feature-specific enterprise policy pref is changed.
  void OnFeatureEnterprisePolicyPrefChanged(UserVisibleFeatureKey feature);

  void StartObservingAccountChanges();

  // signin::IdentityManager::Observer implementation:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;
  void OnIdentityManagerShutdown(
      signin::IdentityManager* identity_manager) override;

  prefs::FeatureOptInState GetPrefState(UserVisibleFeatureKey feature) const;

  // Returns the current validity result for user is eligible to be shown
  // settings for `feature`.
  UserValidityResult GetCurrentUserValidityResult(
      UserVisibleFeatureKey feature) const;

  // Returns a validity result for accounts requiring signin: kValid when signin
  // checks pass, or invalid result indicating the reason if checks fail.
  UserValidityResult PerformSigninChecks() const;

  // Performs settings visibility checks specific to History Search. If passed,
  // `kUnknown` is returned. Otherwise, the corresponding enum for the failed
  // check is returned (i.e. kNotVisibleXXXX).
  SettingsVisibilityResult ShouldHideHistorySearch() const;

  // Initializes the state of the different features at startup.
  void InitializeFeatureSettings();

  // Initializes pref listener to listen to changes to relevant prefs and set up
  // callbacks.
  void InitializePrefListener();

  // Resets the prefs for features that were enabled back to invalid state, when
  // the conditions disallow the features.
  void ResetInvalidFeaturePrefs();

  base::ScopedObservation<signin::IdentityManager,
                          ModelExecutionFeaturesController>
      identity_manager_observation_{this};

  raw_ptr<PrefService> browser_context_profile_service_ = nullptr;

  raw_ptr<signin::IdentityManager> identity_manager_ = nullptr;

  PrefChangeRegistrar pref_change_registrar_;

  bool is_signed_in_ = false;

  // Obtained from the user account capability. Updated whenever sign-in changes
  // or account capability changes.
  bool account_allows_model_execution_features_ = false;

  base::ObserverList<SettingsEnabledObserver> observers_;

  // The PrefService is guaranteed to outlive `this`.
  raw_ptr<PrefService> local_state_;

  // Set of features that are visible to unsigned users.
  base::flat_set<UserVisibleFeatureKey> features_allowed_for_unsigned_user_;

  // Whether this client is a (likely) dogfood client.
  const DogfoodStatus dogfood_status_;

  THREAD_CHECKER(thread_checker_);

  // Used to get `weak_ptr_` to self.
  base::WeakPtrFactory<ModelExecutionFeaturesController> weak_ptr_factory_{
      this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_EXECUTION_FEATURES_CONTROLLER_H_
