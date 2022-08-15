// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TRIGGER_SCRIPTS_TRIGGER_SCRIPT_COORDINATOR_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TRIGGER_SCRIPTS_TRIGGER_SCRIPT_COORDINATOR_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/autofill_assistant/browser/client.h"
#include "components/autofill_assistant/browser/metrics.h"
#include "components/autofill_assistant/browser/onboarding_result.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/service/service_request_sender.h"
#include "components/autofill_assistant/browser/trigger_context.h"
#include "components/autofill_assistant/browser/trigger_scripts/dynamic_trigger_conditions.h"
#include "components/autofill_assistant/browser/trigger_scripts/static_trigger_conditions.h"
#include "components/autofill_assistant/browser/trigger_scripts/trigger_script.h"
#include "components/autofill_assistant/browser/web/web_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace autofill_assistant {
class StarterPlatformDelegate;

// Fetches and coordinates trigger scripts for a specific url. Similar in scope
// and responsibility to a regular |controller|, but for trigger scripts instead
// of regular scripts.
class TriggerScriptCoordinator : public content::WebContentsObserver {
 public:
  // Delegate that shows platform-specific UI.
  class UiDelegate {
   public:
    UiDelegate() = default;
    virtual ~UiDelegate() = default;

    // Displays |proto| to the user.
    virtual void ShowTriggerScript(const TriggerScriptUIProto& proto) = 0;
    // Hides the currently shown trigger script, if any.
    virtual void HideTriggerScript() = 0;
    // Attaches the UiDelegate to |trigger_script_coordinator|. The UiDelegate
    // should notify the coordinator of relevant events.
    virtual void Attach(
        TriggerScriptCoordinator* trigger_script_coordinator) = 0;
    // Detaches the UiDelegate, if attached.
    virtual void Detach() = 0;
  };

  // |web_contents| must outlive this instance.
  TriggerScriptCoordinator(
      base::WeakPtr<StarterPlatformDelegate> starter_delegate,
      content::WebContents* web_contents,
      std::unique_ptr<WebController> web_controller,
      std::unique_ptr<ServiceRequestSender> request_sender,
      const GURL& get_trigger_scripts_server,
      std::unique_ptr<StaticTriggerConditions> static_trigger_conditions,
      std::unique_ptr<DynamicTriggerConditions> dynamic_trigger_conditions,
      ukm::UkmRecorder* ukm_recorder,
      ukm::SourceId deeplink_ukm_source_id);
  ~TriggerScriptCoordinator() override;
  TriggerScriptCoordinator(const TriggerScriptCoordinator&) = delete;
  TriggerScriptCoordinator& operator=(const TriggerScriptCoordinator&) = delete;

  // Retrieves all trigger scripts for |deeplink_url| and starts evaluating
  // their trigger conditions. Invokes |callback| with the result. If a trigger
  // script was shown to the user and accepted by the user, this will also
  // return the shown trigger script and the potentially modified trigger
  // context.
  void Start(const GURL& deeplink_url,
             std::unique_ptr<TriggerContext> trigger_context,
             base::OnceCallback<void(
                 Metrics::TriggerScriptFinishedState result,
                 std::unique_ptr<TriggerContext> trigger_context,
                 absl::optional<TriggerScriptProto> trigger_script)> callback);

  // Stops the currently running trigger script. Hides any currently shown UI
  // (both trigger script UI and onboarding, if applicable) and returns |state|
  // as the reason for stopping in the pending callback.
  void Stop(Metrics::TriggerScriptFinishedState state);

  // Performs |action|. This is usually invoked by the UI as a result of user
  // interactions.
  void PerformTriggerScriptAction(
      TriggerScriptProto::TriggerScriptAction action);

  // Called when the user swipe-dismisses the bottom sheet.
  void OnBottomSheetClosedWithSwipe();

  // Called when the back button is pressed. Returns whether the event was
  // handled or not.
  bool OnBackButtonPressed();

  // Called when the keyboard was shown or hidden.
  void OnKeyboardVisibilityChanged(bool visible);

  // Called when a trigger script was attempted to be shown on screen. This may
  // have failed, for example when trying to show a trigger script after
  // switching from CCT to regular tab.
  void OnTriggerScriptShown(bool success);

  // Called when the tab's interactability has changed. A tab may be
  // non-interactable while invisible and/or while the user is in the
  // tab-switcher.
  void OnTabInteractabilityChanged(bool interactable);

  // Access to the trigger context associated with this coordinator.
  TriggerContext& GetTriggerContext() const;

  // Returns the deeplink that this coordinator was started on.
  const GURL& GetDeeplink() const;

 private:
  friend class TriggerScriptCoordinatorTest;

  // From content::WebContentsObserver.
  void PrimaryPageChanged(content::Page& page) override;
  void OnVisibilityChanged(content::Visibility visibility) override;
  void WebContentsDestroyed() override;

  void StartCheckingTriggerConditions();
  void StopCheckingTriggerConditions();
  void ShowTriggerScript(int index);
  void HideTriggerScript();
  void CheckDynamicTriggerConditions();
  void OnDynamicTriggerConditionsEvaluated(
      bool is_out_of_schedule,
      absl::optional<base::TimeTicks> start_time);
  void OnGetTriggerScripts(
      int http_status,
      const std::string& response,
      const ServiceRequestSender::ResponseInfo& response_info);
  void RegisterExperimentSyntheticFieldTrial(
      const std::vector<std::string>& experiments) const;
  GURL GetCurrentURL() const;
  void OnEffectiveVisibilityChanged();
  void OnOnboardingFinished(bool onboardingShown, OnboardingResult result);
  void OnUiTimeoutReached();

  // Can be invoked to trigger an immediate check of the trigger condition,
  // reusing the dynamic results of the last time. Does nothing if there are no
  // previous results to reuse.
  void RunOutOfScheduleTriggerConditionCheck();

  void RunCallback(TriggerScriptProto::TriggerUIType trigger_ui_type,
                   Metrics::TriggerScriptFinishedState state,
                   const absl::optional<TriggerScriptProto>& trigger_script);

  // Value of trigger_ui_type for the currently visible script, if there is one.
  //
  // When recording a hide or stop action, be sure to capture the type before
  // hiding the script.
  TriggerScriptProto::TriggerUIType GetTriggerUiTypeForVisibleScript() const;

  // Delegate used to access settings and show the onboarding.
  base::WeakPtr<StarterPlatformDelegate> starter_delegate_;

  // Delegate used to show and hide the UI.
  std::unique_ptr<UiDelegate> ui_delegate_;

  // The callback to run once the current trigger script flow has finished.
  base::OnceCallback<void(Metrics::TriggerScriptFinishedState,
                          std::unique_ptr<TriggerContext> trigger_context,
                          absl::optional<TriggerScriptProto>)>
      callback_;

  // The original deeplink to request trigger scripts for.
  GURL deeplink_url_;

  // List of additional domains. If the user leaves the (sub)domain of
  // |deeplink_url_| or |additional_allowed_domains_|, the session stops.
  std::vector<std::string> additional_allowed_domains_;

  // The trigger context for the most recent |Start|. This is stored as a member
  // to allow pausing and resuming the same trigger flow.
  std::unique_ptr<TriggerContext> trigger_context_;

  // Whether the tab is currently visible or not. While invisible or
  // non-interactable (e.g., during tab switching), trigger scripts are hidden
  // and condition evaluation is suspended (see also
  // |web_contents_interactable_|).
  bool web_contents_visible_ = true;

  // Whether the tab is currently interactable or not. While invisible or
  // non-interactable (e.g., during tab switching), trigger scripts are hidden
  // and condition evaluation is suspended (see also |web_contents_visible_|).
  bool web_contents_interactable_ = true;

  // Whether the coordinator is currently checking trigger conditions.
  bool is_checking_trigger_conditions_ = false;

  // Index of the trigger script that is currently being shown. -1 if no script
  // is being shown.
  int visible_trigger_script_ = -1;

  // Used to request trigger scripts from the backend.
  std::unique_ptr<ServiceRequestSender> request_sender_;

  // The URL of the server that should be contacted by |request_sender_|.
  GURL get_trigger_scripts_server_;

  // The web controller to evaluate element conditions.
  std::unique_ptr<WebController> web_controller_;

  // The list of trigger scripts that were fetched from the backend.
  std::vector<std::unique_ptr<TriggerScript>> trigger_scripts_;

  // Evaluate and cache the results for static and dynamic trigger conditions.
  std::unique_ptr<StaticTriggerConditions> static_trigger_conditions_;
  std::unique_ptr<DynamicTriggerConditions> dynamic_trigger_conditions_;

  // The time between consecutive evaluations of dynamic trigger conditions.
  base::TimeDelta trigger_condition_check_interval_ = base::Milliseconds(1000);

  // The number of times the trigger condition may be evaluated. If this reaches
  // 0, the trigger script stops with |TRIGGER_CONDITION_TIMEOUT|.
  // -1 means no limit.
  //
  // This number is defined by the timeout (specified in proto) divided by
  // |trigger_condition_check_interval_|. It resets on tab resume and on UI
  // hidden. While the UI is visible, the number does not decrease.
  int64_t remaining_trigger_condition_evaluations_ = -1;

  // The initial number of evaluations. Used to store the reset value. See
  // |remaining_trigger_condition_evaluations_|.
  int64_t initial_trigger_condition_evaluations_ = -1;

  // The UKM recorder to use for metrics.
  const raw_ptr<ukm::UkmRecorder> ukm_recorder_;

  // The UKM source id to record. This can change over time as the user
  // navigates around, but will always point to a source-id on a supported
  // domain. If the user leaves the supported domain, this will instead point to
  // the last URL still on the supported domain.
  ukm::SourceId ukm_source_id_;

  // Flag to ensure that we only get one TriggerScriptFinished event per run.
  bool finished_state_recorded_ = false;

  // True while the onboarding is being displayed.
  bool waiting_for_onboarding_ = false;

  // Used to automatically hide the UI after a set amount of time, if started.
  // This behaves the same as NOT_NOW; thus, the UI may be reshown the next
  // time the trigger condition matches. The timeout applies each time the UI
  // is shown, but will be disabled if the user starts interacting with the UI.
  base::OneShotTimer ui_timeout_timer_;

  base::WeakPtrFactory<TriggerScriptCoordinator> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TRIGGER_SCRIPTS_TRIGGER_SCRIPT_COORDINATOR_H_
