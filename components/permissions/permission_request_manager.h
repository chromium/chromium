// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PERMISSION_REQUEST_MANAGER_H_
#define COMPONENTS_PERMISSIONS_PERMISSION_REQUEST_MANAGER_H_

#include <algorithm>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/check_is_test.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_prompt.h"
#include "components/permissions/permission_request_queue.h"
#include "components/permissions/permission_ui_selector.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/request_type.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/gfx/geometry/rect.h"

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
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnTabVisibilityChanged(content::Visibility visibility) {}
    virtual void OnPromptAdded() {}
    virtual void OnPromptRemoved() {}
    // Called when recreation of the permission prompt is not possible. It means
    // that `PermissionRequestManager` is ready to display a prompt but the UI
    // layer was not able to display it.
    virtual void OnPromptRecreateViewFailed() {}
    // Called when permission prompt creation was aborted because the current
    // tab is no longer visible, hance it is not possible to display a prompt.
    virtual void OnPromptCreationFailedHiddenTab() {}
    // Called when the current batch of requests have been handled and the
    // prompt is no longer visible. Note that there might be some queued
    // permission requests that will get shown after this. This differs from
    // OnPromptRemoved() in that the prompt may disappear while there are
    // still in-flight requests (e.g. when switching tabs while the prompt is
    // still visible).
    virtual void OnRequestsFinalized() {}

    virtual void OnPermissionRequestManagerDestructed() {}
    virtual void OnNavigation(content::NavigationHandle* navigation_handle) {}

    virtual void OnRequestDecided(permissions::PermissionAction action) {}
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

  // Returns `true` if a permission request is in progress but a prompt view is
  // nullptr.
  bool CanRestorePrompt();

  // Recreates a permission prompt.
  void RestorePrompt();

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
  const std::vector<raw_ptr<PermissionRequest, VectorExperimental>>& Requests()
      override;
  GURL GetRequestingOrigin() const override;
  GURL GetEmbeddingOrigin() const override;
  void Accept() override;
  void AcceptThisTime() override;
  void Deny() override;
  void Dismiss() override;
  void Ignore() override;
  void FinalizeCurrentRequests() override;
  void OpenHelpCenterLink(const ui::Event& event) override;
  void PreIgnoreQuietPrompt() override;
  bool WasCurrentRequestAlreadyDisplayed() override;
  bool ShouldDropCurrentRequestIfCannotShowQuietly() const override;
  bool ShouldCurrentRequestUseQuietUI() const override;
  std::optional<PermissionUiSelector::QuietUiReason> ReasonForUsingQuietUi()
      const override;
  void SetDismissOnTabClose() override;
  void SetPromptShown() override;
  void SetDecisionTime() override;
  void SetManageClicked() override;
  void SetLearnMoreClicked() override;
  base::WeakPtr<PermissionPrompt::Delegate> GetWeakPtr() override;
  content::WebContents* GetAssociatedWebContents() override;
  bool RecreateView() override;

  // Returns the bounds of the active permission prompt view if we're
  // displaying one.
  std::optional<gfx::Rect> GetPromptBubbleViewBoundsInScreen() const;

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

  // Getter for testing.
  const std::vector<std::unique_ptr<PermissionUiSelector>>&
  get_permission_ui_selectors_for_testing() {
    return permission_ui_selectors_;
  }

  void set_view_factory_for_testing(PermissionPrompt::Factory view_factory) {
    view_factory_ = std::move(view_factory);
  }

  PermissionPrompt* view_for_testing() const { return view_.get(); }

  void set_current_request_first_display_time_for_testing(base::Time time) {
    current_request_first_display_time_ = time;
  }

  std::optional<PermissionUmaUtil::PredictionGrantLikelihood>
  prediction_grant_likelihood_for_testing() const {
    return prediction_grant_likelihood_;
  }

  std::optional<permissions::PermissionPromptDisposition>
  current_request_prompt_disposition_for_testing() const {
    return current_request_prompt_disposition_;
  }

  void set_time_to_decision_for_test(base::TimeDelta time_to_decision) {
    time_to_decision_for_test_ = time_to_decision;
  }

  void set_enabled_app_level_notification_permission_for_testing(bool enabled) {
    enabled_app_level_notification_permission_for_testing_ = enabled;
  }

  void set_embedding_origin_for_testing(const GURL& embedding_origin) {
    embedding_origin_for_testing_ = embedding_origin;
  }

  base::ObserverList<Observer>* get_observer_list_for_testing() {
    CHECK_IS_TEST();
    return &observer_list_;
  }

  bool has_pending_requests() {
    return !pending_permission_requests_.IsEmpty();
  }

  void SetHatsShownCallback(base::OnceCallback<void()> callback) override;

  // For permissions that have visible views, we should only record
  // PromptResolved metrics, for ask prompts.
  bool ShouldRecordUmaForCurrentPrompt() const;

 private:
  friend class test::PermissionRequestManagerTestApi;
  friend class content::WebContentsUserData<PermissionRequestManager>;
  FRIEND_TEST_ALL_PREFIXES(PermissionRequestManagerTest, WeakDuplicateRequests);
  FRIEND_TEST_ALL_PREFIXES(PermissionRequestManagerTest,
                           WeakDuplicateRequestsAccept);

  explicit PermissionRequestManager(content::WebContents* web_contents);

  // Defines how to handle the current request, when new requests arrive
  enum class CurrentRequestFate {
    // Keep showing the current request. The incoming requests should not take
    // priority over the current request and will be pushed to pending requests
    // queue.
    kKeepCurrent,

    // Put the current request back to the pending requests queue for displaying
    // later when it returns to the front of the queue.
    kPreempt,

    // Finalize/ignore the current request and show the new request.
    kFinalize
  };

  // Reprioritize the current requests (preempting, finalizing) based on what
  // type of UI has been shown for `requests_` and current pending requests
  // queue.
  // Determine the next request candidate would be prompted later and push the
  // candidate to front of the pending queue.
  // Return true if we keep showing the current request, otherwise return false
  bool ReprioritizeCurrentRequestIfNeeded();

  // Validate the input request. If the request is invalid and
  // |should_finalize| is set, cancel and remove it from *_map_ and *_set_.
  // Return true if the request is valid, otherwise false.
  bool ValidateRequest(PermissionRequest* request, bool should_finalize = true);

  // Adds `request` into `pending_permission_requests_`, and request's
  // `source_frame` into `request_sources_map_`.
  void QueueRequest(content::RenderFrameHost* source_frame,
                    PermissionRequest* request);

  // Because the requests are shown in a different order for Normal and Quiet
  // Chip, pending requests are returned back to pending_permission_requests_ to
  // process them after the new requests.
  void PreemptAndRequeueCurrentRequest();

  // If a request isn't already in progress, dequeue and show the request
  // prompt.
  void DequeueRequestIfNeeded();

  // Schedule a call to |DequeueRequestIfNeeded|. Is needed to ensure requests
  // that can be grouped together have time to all be added to the queue.
  void ScheduleDequeueRequestIfNeeded();

  // Shows the prompt for a request that has just been dequeued, or re-show a
  // prompt after switching tabs away and back.
  void ShowPrompt();

  // Delete the view object
  void DeletePrompt();

  // Finalize request.
  void ResetViewStateForCurrentRequest();

  // Records metrics and informs embargo and autoblocker about the requests
  // being decided. Based on |view_->ShouldFinalizeRequestAfterDecided()| it
  // will also call |FinalizeCurrentRequests()|. Otherwise a separate
  // |FinalizeCurrentRequests()| call must be made to release the |view_|.
  void CurrentRequestsDecided(PermissionAction permission_action);

  // Cancel all pending or active requests and destroy the PermissionPrompt if
  // one exists. This is called if the WebContents is destroyed or navigates its
  // main frame.
  void CleanUpRequests();

  // Searches |requests_| and |pending_permission_requests_| - but *not*
  // |duplicate_requests_| - for a request matching |request|, and returns the
  // matching request, or |nullptr| if no match. Note that the matching request
  // may or may not be the same object as |request|.
  PermissionRequest* GetExistingRequest(PermissionRequest* request) const;

  // Returns an iterator into |duplicate_requests_|, points the matching list,
  // or duplicate_requests_.end() if no match. The matching list contains all
  // the weak requests which are duplicate of the given |request| (see
  // |IsDuplicateOf|)
  using WeakPermissionRequestList =
      std::list<std::list<base::WeakPtr<PermissionRequest>>>;
  WeakPermissionRequestList::iterator FindDuplicateRequestList(
      PermissionRequest* request);

  // Trigger |visitor| for each live weak request which matches the given
  // request (see |IsDuplicateOf|) in the |duplicate_requests_|. Returns an
  // iterator into |duplicate_requests_|, points the matching list, or
  // duplicate_requests_.end() if no match.
  using DuplicateRequestVisitor =
      base::RepeatingCallback<void(const base::WeakPtr<PermissionRequest>&)>;
  WeakPermissionRequestList::iterator VisitDuplicateRequests(
      DuplicateRequestVisitor visitor,
      PermissionRequest* request);

  // Calls PermissionGranted on a request and all its duplicates.
  void PermissionGrantedIncludingDuplicates(PermissionRequest* request,
                                            bool is_one_time);
  // Calls PermissionDenied on a request and all its duplicates.
  void PermissionDeniedIncludingDuplicates(PermissionRequest* request);
  // Calls Cancelled on a request and all its duplicates.
  void CancelledIncludingDuplicates(PermissionRequest* request,
                                    bool is_final_decision = true);
  // Calls RequestFinished on a request and all its duplicates.
  void RequestFinishedIncludingDuplicates(PermissionRequest* request);

  void NotifyTabVisibilityChanged(content::Visibility visibility);
  void NotifyPromptAdded();
  void NotifyPromptRemoved();
  void NotifyPromptRecreateFailed();
  void NotifyPromptCreationFailedHiddenTab();
  void NotifyRequestDecided(permissions::PermissionAction permission_action);

  void StorePermissionActionForUMA(const GURL& origin,
                                   RequestType request_type,
                                   PermissionAction permission_action);

  void OnPermissionUiSelectorDone(size_t selector_index,
                                  const UiDecision& decision);

  PermissionPromptDisposition DetermineCurrentRequestUIDisposition();
  PermissionPromptDispositionReason
  DetermineCurrentRequestUIDispositionReasonForUMA();

  void LogWarningToConsole(const char* message);

  void DoAutoResponseForTesting();

  void PreIgnoreQuietPromptInternal();

  // Returns true if there is a request in progress that is initiated by an
  // embedded permission element.
  bool IsCurrentRequestEmbeddedPermissionElementInitiated() const;

  // Returns true when the current request should be finalized together with the
  // permission decision.
  bool ShouldFinalizeRequestAfterDecided(PermissionAction action) const;

  // Calculate and record the PermissionEmbargoStatus.
  PermissionEmbargoStatus RecordActionAndGetEmbargoStatus(
      content::BrowserContext* browser_context,
      PermissionRequest* request,
      PermissionAction permission_action);

  // Take a snapshot of the content setting status for the current requests,
  // which can change based on the user's decision. Used in HaTS as a filter.
  // This defaults to "DEFAULT" if there's no ContentSettingsType associated
  // with the PermissionType.
  void SetCurrentRequestsInitialStatuses();

  ContentSetting GetRequestInitialStatus(PermissionRequest* request);

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
  std::optional<permissions::PermissionPromptDisposition>
      current_request_prompt_disposition_;

  // We only show new prompts when |tab_is_hidden_| is false.
  bool tab_is_hidden_;

  // The request (or requests) that the user is currently being prompted for.
  // When this is non-empty, the |view_| is generally non-null as long as the
  // tab is visible.
  std::vector<raw_ptr<PermissionRequest, VectorExperimental>> requests_;

  struct PermissionRequestSource {
    content::GlobalRenderFrameHostId requesting_frame_id;

    bool IsSourceFrameInactiveAndDisallowActivation() const;
  };

  PermissionRequestQueue pending_permission_requests_;

  // Stores the weak pointers of duplicated requests in a 2D list.
  WeakPermissionRequestList duplicate_requests_;

  // Maps each PermissionRequest currently in |requests_| or
  // |pending_permission_requests_| to which RenderFrameHost it originated from.
  // Note that no date is stored for |duplicate_requests_|.
  std::map<PermissionRequest*, PermissionRequestSource> request_sources_map_;

  // Sequence of requests from pending queue will be marked as validated, when
  // we are extracting a group of requests from the queue to show to user. This
  // is an immature solution to avoid an infinitive loop of preempting, we would
  // not prempt a request if the incoming request is already validated.
  std::set<raw_ptr<PermissionRequest, SetExperimental>> validated_requests_set_;

  base::ObserverList<Observer> observer_list_;
  AutoResponseType auto_response_for_test_ = NONE;

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
  std::vector<std::optional<PermissionUiSelector::Decision>>
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
  std::optional<UiDecision> current_request_ui_to_use_;

  // The likelihood value returned by the Web Permission Predictions Service,
  // to be recoreded in UKM.
  std::optional<PermissionUmaUtil::PredictionGrantLikelihood>
      prediction_grant_likelihood_;

  // Status of the decision made by the Web Permission Prediction Service, if
  // it was held back or not.
  std::optional<bool> was_decision_held_back_;

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

  // Whether the permission prompt was shown for the current request.
  bool did_show_prompt_ = false;

  // When the user made any decision for the current |requests_|, or zero if not
  // at all.
  base::Time current_request_decision_time_;

  bool did_click_manage_ = false;

  bool did_click_learn_more_ = false;

  // Whether the current request can be preempted or not. This is set when
  // callbacks are being issued to prevent potential re-entrant behavior of
  // those callbacks requesting a permission that would preempt the current one
  // and thus invalidate the iterator being used to issue the callback.
  bool can_preempt_current_request_ = true;

  std::optional<base::TimeDelta> time_to_decision_for_test_;

  std::optional<bool> enabled_app_level_notification_permission_for_testing_;

  std::optional<GURL> embedding_origin_for_testing_;

  // A timer is used to pre-ignore the permission request if it's been displayed
  // as a quiet chip.
  base::OneShotTimer preignore_timer_;

  std::optional<base::OnceCallback<void()>> hats_shown_callback_;

  // Holds the position of the current prompt, only relevant for permission
  // element prompts.
  std::optional<feature_params::PermissionElementPromptPosition>
      current_request_pepc_prompt_position_;

  // Holds the initial statuses of the current requests, one for each request in
  // |requests_|.
  std::map<PermissionRequest*, ContentSetting>
      current_requests_initial_statuses_;

  base::WeakPtrFactory<PermissionRequestManager> weak_factory_{this};
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_PERMISSION_REQUEST_MANAGER_H_
