// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PERMISSION_REQUEST_MANAGER_H_
#define COMPONENTS_PERMISSIONS_PERMISSION_REQUEST_MANAGER_H_

#include <algorithm>
#include <utility>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "components/permissions/permission_prompt.h"
#include "components/permissions/permission_request_queue.h"
#include "components/permissions/permission_ui_selector.h"
#include "components/permissions/permission_uma_util.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class GURL;

namespace content {
class RenderFrameHost;
}

namespace test {
class PermissionRequestManagerTestApi;
}

namespace permissions {
class PermissionRequest;
enum class PermissionAction;
enum class PermissionPromptDisposition;
enum class PermissionPromptDispositionReason;

// The message to be printed in the Developer Tools console when the quiet
// notification permission prompt UI is shown on sites with abusive permission
// request flows.
extern const char kAbusiveNotificationRequestsEnforcementMessage[];

// The message to be printed in the Developer Tools console when the site is on
// the warning list for abusive permission request flows.
extern const char kAbusiveNotificationRequestsWarningMessage[];

// The message to be printed in the Developer Tools console when the site is on
// the blocking list for showing abusive notification content.
extern const char kAbusiveNotificationContentEnforcementMessage[];

// The message to be printed in the Developer Tools console when the site is on
// the warning list for showing abusive notification content.
extern const char kAbusiveNotificationContentWarningMessage[];

// Provides access to permissions bubbles. Allows clients to add a request
// callback interface to the existing permission bubble configuration.
// Depending on the situation and policy, that may add new UI to an existing
// permission bubble, create and show a new permission bubble, or provide no
// visible UI action at all. (In that case, the request will be immediately
// informed that the permission request failed.)
//
// A PermissionRequestManager is associated with a particular WebContents.
// Requests attached to a particular WebContents' PBM must outlive it.
//
// The PermissionRequestManager should be addressed on the UI thread.
class PermissionRequestManager
    : public content::WebContentsObserver,
      public content::WebContentsUserData<PermissionRequestManager>,
      public PermissionPrompt::Delegate {
 public:
  class Observer {
   public:
    virtual void OnBubbleAdded() {}
    virtual void OnBubbleRemoved() {}
    // Called when the current batch of requests have been handled and the
    // bubble is no longer visible. Note that there might be some queued
    // permission requests that will get shown after this. This differs from
    // OnBubbleRemoved() in that the bubble may disappear while there are still
    // in-flight requests (e.g. when switching tabs while the bubble is still
    // visible).
    virtual void OnRequestsFinalized() {}

    virtual void OnPermissionRequestManagerDestructed() {}

   protected:
    virtual ~Observer() = default;
  };

  enum AutoResponseType { NONE, ACCEPT_ONCE, ACCEPT_ALL, DENY_ALL, DISMISS };

  using UiDecision = PermissionUiSelector::Decision;
  using QuietUiReason = PermissionUiSelector::QuietUiReason;
  using WarningReason = PermissionUiSelector::WarningReason;

  ~PermissionRequestManager() override;

  // Adds a new request to the permission bubble. Ownership of the request
  // remains with the caller. The caller must arrange for the request to
  // outlive the PermissionRequestManager. If a bubble is visible when this
  // call is made, the request will be queued up and shown after the current
  // bubble closes. A request with message text identical to an outstanding
  // request will be merged with the outstanding request, and will have the same
  // callbacks called as the outstanding request.
  void AddRequest(content::RenderFrameHost* source_frame,
                  PermissionRequest* request);

  // Will reposition the bubble (may change parent if necessary).
  void UpdateAnchor();

  // For observing the status of the permission bubble manager.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  bool IsRequestInProgress() const;

  // Do NOT use this methods in production code. Use this methods in browser
  // tests that need to accept or deny permissions when requested in
  // JavaScript. Your test needs to set this appropriately, and then the bubble
  // will proceed as desired as soon as Show() is called.
  void set_auto_response_for_test(AutoResponseType response) {
    auto_response_for_test_ = response;
  }

  // WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DocumentOnLoadCompletedInPrimaryMainFrame() override;
  void DOMContentLoaded(content::RenderFrameHost* render_frame_host) override;
  void WebContentsDestroyed() override;
  void OnVisibilityChanged(content::Visibility visibility) override;

  // PermissionPrompt::Delegate:
  const std::vector<PermissionRequest*>& Requests() override;
  GURL GetRequestingOrigin() const override;
  GURL GetEmbeddingOrigin() const override;
  void Accept() override;
  void AcceptThisTime() override;
  void Deny() override;
  void Dismiss() override;
  void Ignore() override;
  bool WasCurrentRequestAlreadyDisplayed() override;
  bool ShouldDropCurrentRequestIfCannotShowQuietly() const override;
  bool ShouldCurrentRequestUseQuietUI() const override;
  absl::optional<PermissionUiSelector::QuietUiReason> ReasonForUsingQuietUi()
      const override;
  void SetDismissOnTabClose() override;
  void SetBubbleShown() override;
  void SetDecisionTime() override;
  void SetManageClicked() override;
  void SetLearnMoreClicked() override;
  base::WeakPtr<PermissionPrompt::Delegate> GetWeakPtr() override;
  bool RecreateView() override;

  void set_manage_clicked() { did_click_manage_ = true; }
  void set_learn_more_clicked() { did_click_learn_more_ = true; }

  void set_web_contents_supports_permission_requests(
      bool web_contents_supports_permission_requests) {
    web_contents_supports_permission_requests_ =
        web_contents_supports_permission_requests;
  }

  // For testing only, used to override the default UI selectors and instead use
  // a new one.
  void set_permission_ui_selector_for_testing(
      std::unique_ptr<PermissionUiSelector> selector) {
    clear_permission_ui_selector_for_testing();
    add_permission_ui_selector_for_testing(std::move(selector));
  }

  // For testing only, used to add a new selector without overriding the
  // existing ones.
  void add_permission_ui_selector_for_testing(
      std::unique_ptr<PermissionUiSelector> selector) {
    permission_ui_selectors_.emplace_back(std::move(selector));
  }

  // For testing only, clear the existing ui selectors.
  void clear_permission_ui_selector_for_testing() {
    permission_ui_selectors_.clear();
  }

  void set_view_factory_for_testing(PermissionPrompt::Factory view_factory) {
    view_factory_ = std::move(view_factory);
  }

  PermissionPrompt* view_for_testing() { return view_.get(); }

  void set_current_request_first_display_time_for_testing(base::Time time) {
    current_request_first_display_time_ = time;
  }

  absl::optional<PermissionUmaUtil::PredictionGrantLikelihood>
  prediction_grant_likelihood_for_testing() {
    return prediction_grant_likelihood_;
  }

  absl::optional<permissions::PermissionPromptDisposition>
  current_request_prompt_disposition_for_testing() {
    return current_request_prompt_disposition_;
  }

  void set_time_to_decision_for_test(base::TimeDelta time_to_decision) {
    time_to_decision_for_test_ = time_to_decision;
  }

  void set_enabled_app_level_notification_permission_for_testing(bool enabled) {
    enabled_app_level_notification_permission_for_testing_ = enabled;
  }

 private:
  friend class test::PermissionRequestManagerTestApi;
  friend class content::WebContentsUserData<PermissionRequestManager>;

  explicit PermissionRequestManager(content::WebContents* web_contents);

  enum class CurrentRequestFate { KeepCurrent, Preempt, Finalize };

  // Returns `CurrentRequestFate` based on what type of UI has been shown for
  // `requests_`.
  CurrentRequestFate GetCurrentRequestFateInFaceOfNewRequest(
      PermissionRequest* request);

  // Adds `request` into `pending_permission_requests_`, and request's
  // `source_frame` into `request_sources_map_`.
  void QueueRequest(content::RenderFrameHost* source_frame,
                    PermissionRequest* request);

  // Because the requests are shown in a different order for Normal and Quiet
  // Chip, pending requests are returned back to pending_permission_requests_ to
  // process them after the new requests.
  void PreemptAndRequeueCurrentRequest();

  // Posts a task which will allow the bubble to become visible.
  void ScheduleShowBubble();

  // If a request isn't already in progress, deque and schedule showing the
  // request.
  void DequeueRequestIfNeeded();

  // Schedule a call to dequeue request. Is needed to ensure requests that can
  // be grouped together have time to all be added to the queue.
  void ScheduleDequeueRequestIfNeeded();

  // Will determine if it's possible and necessary to dequeue a new request.
  bool ShouldDequeueNewRequest();

  // Shows the bubble for a request that has just been dequeued, or re-show a
  // bubble after switching tabs away and back.
  void ShowBubble();

  // Delete the view object
  void DeleteBubble();

  // Finalize request.
  void ResetViewStateForCurrentRequest();

  // Delete the view object, finalize requests, asynchronously show a queued
  // request if present.
  void FinalizeCurrentRequests(PermissionAction permission_action);

  // Cancel all pending or active requests and destroy the PermissionPrompt if
  // one exists. This is called if the WebContents is destroyed or navigates its
  // main frame.
  void CleanUpRequests();

  // Searches |requests_| and |pending_permission_requests_| - but *not*
  // |duplicate_requests_| - for a request matching |request|, and returns the
  // matching request, or |nullptr| if no match. Note that the matching request
  // may or may not be the same object as |request|.
  PermissionRequest* GetExistingRequest(PermissionRequest* request);

  // Calls PermissionGranted on a request and all its duplicates.
  void PermissionGrantedIncludingDuplicates(PermissionRequest* request,
                                            bool is_one_time);
  // Calls PermissionDenied on a request and all its duplicates.
  void PermissionDeniedIncludingDuplicates(PermissionRequest* request);
  // Calls Cancelled on a request and all its duplicates.
  void CancelledIncludingDuplicates(PermissionRequest* request);
  // Calls RequestFinished on a request and all its duplicates.
  void RequestFinishedIncludingDuplicates(PermissionRequest* request);

  void NotifyBubbleAdded();
  void NotifyBubbleRemoved();

  void OnPermissionUiSelectorDone(size_t selector_index,
                                  const UiDecision& decision);

  PermissionPromptDisposition DetermineCurrentRequestUIDisposition();
  PermissionPromptDispositionReason
  DetermineCurrentRequestUIDispositionReasonForUMA();

  void LogWarningToConsole(const char* message);

  void DoAutoResponseForTesting();

  // Factory to be used to create views when needed.
  PermissionPrompt::Factory view_factory_;

  // The UI surface for an active permission prompt if we're displaying one.
  // On Desktop, we destroy this upon tab switching, while on Android we keep
  // the object alive. The infobar system hides the actual infobar UI and modals
  // prevent tab switching.
  std::unique_ptr<PermissionPrompt> view_;

  // The disposition for the currently active permission prompt, if any.
  // Recorded separately because the `view_` might not be available at prompt
  // resolution in order to determine the disposition.
  absl::optional<permissions::PermissionPromptDisposition>
      current_request_prompt_disposition_;

  // We only show new prompts when |tab_is_hidden_| is false.
  bool tab_is_hidden_;

  // The request (or requests) that the user is currently being prompted for.
  // When this is non-empty, the |view_| is generally non-null as long as the
  // tab is visible.
  std::vector<PermissionRequest*> requests_;

  struct PermissionRequestSource {
    int render_process_id;
    int render_frame_id;

    bool IsSourceFrameInactiveAndDisallowActivation() const;
  };

  PermissionRequestQueue pending_permission_requests_;

  // Maps from the first request of a kind to subsequent requests that were
  // duped against it.
  std::unordered_multimap<PermissionRequest*, PermissionRequest*>
      duplicate_requests_;

  // Maps each PermissionRequest currently in |requests_| or
  // |pending_permission_requests_| to which RenderFrameHost it originated from.
  // Note that no date is stored for |duplicate_requests_|.
  std::map<PermissionRequest*, PermissionRequestSource> request_sources_map_;

  base::ObserverList<Observer>::Unchecked observer_list_;
  AutoResponseType auto_response_for_test_;

  // Suppress notification permission prompts in this tab, regardless of the
  // origin requesting the permission.
  bool is_notification_prompt_cooldown_active_ = false;

  // A vector of selectors which decide if the quiet prompt UI should be used
  // to display permission requests. Sorted from the highest priority to the
  // lowest priority selector.
  std::vector<std::unique_ptr<PermissionUiSelector>> permission_ui_selectors_;

  // Holds the decisions returned by selectors. Needed in case a lower priority
  // selector returns a decision first and we need to wait for the decisions of
  // higher priority selectors before making use of it.
  std::vector<absl::optional<PermissionUiSelector::Decision>>
      selector_decisions_;

  // Whether the view for the current |requests_| has been shown to the user at
  // least once.
  bool current_request_already_displayed_ = false;

  // When the view for the current |requests_| has been first shown to the user,
  // or zero if not at all.
  base::Time current_request_first_display_time_;

  // Whether to use the normal or quiet UI to display the current permission
  // |requests_|, and whether to show warnings. This will be nullopt if we are
  // still waiting on the result from |permission_ui_selectors_|.
  absl::optional<UiDecision> current_request_ui_to_use_;

  // The likelihood value returned by the Web Permission Predictions Service,
  // to be recoreded in UKM.
  absl::optional<PermissionUmaUtil::PredictionGrantLikelihood>
      prediction_grant_likelihood_;

  // Status of the decision made by the Web Permission Prediction Service, if
  // it was held back or not.
  absl::optional<bool> was_decision_held_back_;

  // True when the prompt is being temporary destroyed to be recreated for the
  // correct browser or when the tab is hidden. In those cases, callbacks from
  // the bubble itself should be ignored.
  bool ignore_callbacks_from_prompt_ = false;

  // Whether the web contents associated with this request manager supports
  // permission prompts.
  bool web_contents_supports_permission_requests_ = true;

  // Whether the current request should be dismissed if the current tab is
  // closed.
  bool should_dismiss_current_request_ = false;

  // Whether the permission prompt bubble was shown for the current request.
  bool did_show_bubble_ = false;

  // When the user made any decision for the current |requests_|, or zero if not
  // at all.
  base::Time current_request_decision_time_;

  bool did_click_manage_ = false;

  bool did_click_learn_more_ = false;

  absl::optional<base::TimeDelta> time_to_decision_for_test_;

  absl::optional<bool> enabled_app_level_notification_permission_for_testing_;

  base::WeakPtrFactory<PermissionRequestManager> weak_factory_{this};
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_PERMISSION_REQUEST_MANAGER_H_
