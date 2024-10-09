// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SCALABLE_IPH_SCALABLE_IPH_H_
#define CHROMEOS_ASH_COMPONENTS_SCALABLE_IPH_SCALABLE_IPH_H_

#include <optional>
#include <ostream>
#include <vector>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/scalable_iph/logger.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph_constants.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph_delegate.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/keyed_service/core/keyed_service.h"

namespace scalable_iph {

// `ScalableIph` provides a scalable way to deliver IPHs.
//
// - Scalable: we provide a scalable way by building this framework on top of
// the feature engagement framework. A developer can set up an IPH without
// modifying a binary. See feature engagement doc for details about its
// flexibility: //components/feature_engagement/README.md.
//
// - IPH: in-product-help.
//
// Class diagram:
// =============================================================================
//
// //chromeos/ash/components       | //chrome/browser/ash
// -----------------------------------------------------------------------------
//
// |-------------|
// |             |
// |             |                 |---------------------|              |-----|
// |             | -[TriggerIph]-> |                     | -[ShowUI]--> |     |
// |             | ---[Action]---> | ScalableIphDelegate | -[OpenUrl]-> | Ash |
// |             | <--[Observer]-- |                     | <-[Events]-- |     |
// |             |                 |---------------------|              |-----|
// |             |                                                         |
// |             |                 |---------|                             |
// | ScalableIph | <---[Action]--- | HelpApp |                             |
// |             |                 |---------|                             |
// |             |                                                        \|/
// |             |                                               |------------|
// |             | <-------------------[Action]----------------- | IphSession |
// |             |                                               |------------|
// |             |                                                         |
// |             |                                                        \|/
// |             |                          |---------------------------------|
// |             | -------[Interact]------> | //components/feature_engagement |
// |-------------|                          |---------------------------------|
//
// ScalableIph: The main component of Scalable Iph framework. This class checks
//              trigger conditions and parse Scalable Iph custom fields, e.g.
//              Custom conditions.
// ScalableIphDelegate: A delegate class for `ScalableIph` to delegate its tasks
//                      to Ash or Chrome. An implementation of
//                      `ScalableIphDelegate` will be in //chrome/browser/ash.
//                      Delegated tasks will be:
//                      - Show an IPH UI, e.g. Notification.
//                      - Observe events, e.g. Network connection.
//                      - Perform actions, e.g. Open a URL.
// IphSession: An object for managing a single IPH session. If an UI is opened
//             by `ScalableIph` (e.g. Notification, Bubble), `IphSession` is
//             passed to those code for `ScalableIph` to manage an IPH session
//             and for those UIs to perform actions. `IphSession` can interact
//             with a `feature_engagement::Tracker` directly as it holds a
//             reference to it. But it has to delegate actions to `ScalableIph`
//             as it is in //chromeos/ash/components. `ScalableIph` delegates
//             them again to `ScalableIphDelegate`.
//
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SCALABLE_IPH) ScalableIph
    : public KeyedService,
      public ScalableIphDelegate::Observer,
      public IphSession::Delegate {
 public:
  // List of events ScalableIph supports.
  enum class Event {
    kFiveMinTick = 0,
    kUnlocked,
    kAppListShown,
    kAppListItemActivationYouTube,
    kAppListItemActivationGoogleDocs,
    kAppListItemActivationGooglePhotosWeb,
    kOpenPersonalizationApp,
    kShelfItemActivationYouTube,
    kShelfItemActivationGoogleDocs,
    kShelfItemActivationGooglePhotosWeb,
    kShelfItemActivationGooglePhotosAndroid,
    kShelfItemActivationGooglePlay,
    kAppListItemActivationGooglePlayStore,
    kAppListItemActivationGooglePhotosAndroid,
    kPrintJobCreated,
    kGameWindowOpened,
  };

  // Returns true if any iph feature flag is enabled. Otherwise false.
  static bool IsAnyIphFeatureEnabled();

  // Force enable `IsAnyIphFeatureEnabled` check for testing. Note that no
  // actual iph feature flag gets enabled by this.
  static void ForceEnableIphFeatureForTesting();

  ScalableIph(feature_engagement::Tracker* tracker,
              std::unique_ptr<ScalableIphDelegate> delegate,
              std::unique_ptr<Logger> logger);

  void RecordEvent(Event event);

  Logger* GetLogger();

  ScalableIphDelegate* delegate_for_testing() { return delegate_.get(); }

  // KeyedService:
  ~ScalableIph() override;
  void Shutdown() override;

  // ScalableIphDelegate::Observer:
  void OnConnectionChanged(bool online) override;
  void OnSessionStateChanged(ScalableIphDelegate::SessionState state) override;
  void OnSuspendDoneWithoutLockScreen() override;
  void OnAppListVisibilityChanged(bool shown) override;
  void OnHasSavedPrintersChanged(bool has_saved_printers) override;
  void OnPhoneHubOnboardingEligibleChanged(
      bool phonehub_onboarding_eligible) override;

  // IphSession::Delegate:
  void PerformActionForIphSession(ActionType action_type) override;

  void OverrideFeatureListForTesting(
      const std::vector<raw_ptr<const base::Feature, VectorExperimental>>
          features);
  void OverrideTaskRunnerForTesting(
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  // Called for a user action in the help app. All the logging related to
  // help app action events will be done here before calling `PerformAction`.
  void PerformActionForHelpApp(ActionType action_type);

  // Perform `action_type` as a result of a user action, e.g. A link click in a
  // help app, etc. This notifies a corresponding IPH event to the feature
  // engagement framework.
  //
  // UIs which were initiated with `IphSession` (e.g. Notification, Bubble)
  // should use `IphSession::PerformAction` instead of this method.
  void PerformAction(ActionType action_type);

  // `SyncedPrintersManager` stores its observers in `ObserverListThreadSafe`,
  // which invokes observers via `TaskRunner`. Test code can set a closure to
  // this method to wait an observer of `ScalableIph` being called.
  //
  // Note:
  // We cannot wait this by registering another observer in a test and wait it.
  // Observers are stored in an unordered map. There is no guarantee on the
  // order of calls.
  void SetHasSavedPrintersChangedClosureForTesting(
      base::RepeatingClosure has_saved_printers_closure);

  // Maybe record an app list item or a shelf item activation of `id`.
  void MaybeRecordAppListItemActivation(const std::string& id);
  void MaybeRecordShelfItemActivationById(const std::string& id);

  // Returns true if the help app should be pinned to the bottom shelf.
  bool ShouldPinHelpAppToShelf();

  static const std::vector<raw_ptr<const base::Feature, VectorExperimental>>&
  GetFeatureListConstantForTesting();

 private:
  void EnsureTimerStarted();
  void RecordTimeTickEvent();
  void RecordUnlockedEvent();
  void RecordEventInternal(Event event, bool init_success);
  void CheckTriggerConditionsOnInitSuccess(bool init_success);
  void CheckTriggerConditions(
      const std::optional<ScalableIph::Event>& trigger_event);

  // Check all custom conditions assigned to `feature`. Returns true if all
  // conditions are valid and satisfied. Otherwise false including an invalid
  // config case.
  bool CheckCustomConditions(const base::Feature& feature,
                             const std::optional<Event>& trigger_event);
  bool CheckTriggerEvent(const base::Feature& feature,
                         const std::optional<Event>& trigger_event);
  bool CheckNetworkConnection(const base::Feature& feature);
  bool CheckClientAge(const base::Feature& feature);
  bool CheckHasSavedPrinters(const base::Feature& feature);
  bool CheckPhoneHubOnboardingEligible(const base::Feature& feature);

  const std::vector<raw_ptr<const base::Feature, VectorExperimental>>&
  GetFeatureList() const;

  raw_ptr<feature_engagement::Tracker> tracker_;
  std::unique_ptr<ScalableIphDelegate> delegate_;
  base::RepeatingTimer timer_;
  bool online_ = false;
  ScalableIphDelegate::SessionState session_state_ =
      ScalableIphDelegate::SessionState::kUnknownInitialValue;
  bool has_saved_printers_ = false;
  bool phonehub_onboarding_eligible_ = false;
  std::unique_ptr<Logger> logger_;

  base::RepeatingClosure has_saved_printers_closure_for_testing_;
  std::vector<raw_ptr<const base::Feature, VectorExperimental>>
      feature_list_for_testing_;

  base::ScopedObservation<ScalableIphDelegate, ScalableIph>
      delegate_observation_{this};

  base::WeakPtrFactory<ScalableIph> weak_ptr_factory_{this};
};

std::ostream& operator<<(std::ostream& out, ScalableIph::Event event);

}  // namespace scalable_iph

#endif  // CHROMEOS_ASH_COMPONENTS_SCALABLE_IPH_SCALABLE_IPH_H_
