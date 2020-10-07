// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PERMISSION_REQUEST_MANAGER_H_
#define COMPONENTS_PERMISSIONS_PERMISSION_REQUEST_MANAGER_H_

#include <unordered_map>
#include <utility>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/permissions/notification_permission_ui_selector.h"
#include "components/permissions/permission_prompt.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

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

   protected:
    virtual ~Observer() = default;
  };

  enum AutoResponseType { NONE, ACCEPT_ALL, DENY_ALL, DISMISS };

  using UiDecision = NotificationPermissionUiSelector::Decision;
  using QuietUiReason = NotificationPermissionUiSelector::QuietUiReason;
  using WarningReason = NotificationPermissionUiSelector::WarningReason;

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
  void UpdateAnchorPosition();

  // For observing the status of the permission bubble manager.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Notification permission requests might use a quiet UI when the
  // "quiet-notification-prompts" feature is enabled. This is done either
  // directly by the user in notifications settings, or via automatic logic that
  // might trigger the current request to use the quiet UI.
  bool ShouldCurrentRequestUseQuietUI() const;

  // If |ShouldCurrentRequestUseQuietUI| return true, this will provide a reason
  // as to why the quiet UI needs to be used.
  QuietUiReason ReasonForUsingQuietUi() const;

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
  void DocumentOnLoadCompletedInMainFrame() override;
  void DOMContentLoaded(content::RenderFrameHost* render_frame_host) override;
  void WebContentsDestroyed() override;
  void OnVisibilityChanged(content::Visibility visibility) override;

  // PermissionPrompt::Delegate:
  const std::vector<PermissionRequest*>& Requests() override;
  GURL GetEmbeddingOrigin() const override;
  void Accept() override;
  void Deny() override;
  void Closing() override;
  bool WasCurrentRequestAlreadyDisplayed() override;

  void set_web_contents_supports_permission_requests(
      bool web_contents_supports_permission_requests) {
    web_contents_supports_permission_requests_ =
        web_contents_supports_permission_requests;
  }

  // For testing only, used to override the default UI selector.
  void set_notification_permission_ui_selector_for_testing(
      std::unique_ptr<NotificationPermissionUiSelector> selector) {
    notification_permission_ui_selector_ = std::move(selector);
  }

  void set_view_factory_for_testing(PermissionPrompt::Factory view_factory) {
    view_factory_ = std::move(view_factory);
  }

 private:
  friend class test::PermissionRequestManagerTestApi;
  friend class content::WebContentsUserData<PermissionRequestManager>;

  explicit PermissionRequestManager(content::WebContents* web_contents);

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

  // Delete the view object, finalize requests, asynchronously show a queued
  // request if present.
  void FinalizeBubble(PermissionAction permission_action);

  // Cancel all pending or active requests and destroy the PermissionPrompt if
  // one exists. This is called if the WebContents is destroyed or navigates its
  // main frame.
  void CleanUpRequests();

  // Searches |requests_|, |queued_requests_| and |queued_frame_requests_| - but
  // *not* |duplicate_requests_| - for a request matching |request|, and returns
  // the matching request, or |nullptr| if no match. Note that the matching
  // request may or may not be the same object as |request|.
  PermissionRequest* GetExistingRequest(PermissionRequest* request);

  // Calls PermissionGranted on a request and all its duplicates.
  void PermissionGrantedIncludingDuplicates(PermissionRequest* request);
  // Calls PermissionDenied on a request and all its duplicates.
  void PermissionDeniedIncludingDuplicates(PermissionRequest* request);
  // Calls Cancelled on a request and all its duplicates.
  void CancelledIncludingDuplicates(PermissionRequest* request);
  // Calls RequestFinished on a request and all its duplicates.
  void RequestFinishedIncludingDuplicates(PermissionRequest* request);

  void NotifyBubbleAdded();
  void NotifyBubbleRemoved();

  void OnSelectedUiToUseForNotifications(const UiDecision& decision);

  PermissionPromptDisposition DetermineCurrentRequestUIDispositionForUMA();

  void LogWarningToConsole(const char* message);

  void DoAutoResponseForTesting();

  int CountQueuedPermissionRequests(PermissionRequest* request);

  // Factory to be used to create views when needed.
  PermissionPrompt::Factory view_factory_;

  // The UI surface for an active permission prompt if we're displaying one.
  // On Desktop, we destroy this upon tab switching, while on Android we keep
  // the object alive. The infobar system hides the actual infobar UI and modals
  // prevent tab switching.
  std::unique_ptr<PermissionPrompt> view_;
  // We only show new prompts when |tab_is_hidden_| is false.
  bool tab_is_hidden_;

  // The request (or requests) that the user is currently being prompted for.
  // When this is non-empty, the |view_| is generally non-null as long as the
  // tab is visible.
  std::vector<PermissionRequest*> requests_;

  struct RequestAndSource {
    int render_process_id;
    int render_frame_id;
    PermissionRequest* request;

    bool IsSourceFrameInactiveAndDisallowReactivation() const;
  };

  base::circular_deque<RequestAndSource> queued_requests_;
  // Maps from the first request of a kind to subsequent requests that were
  // duped against it.
  std::unordered_multimap<PermissionRequest*, PermissionRequest*>
      duplicate_requests_;

  base::ObserverList<Observer>::Unchecked observer_list_;
  AutoResponseType auto_response_for_test_;

  // Suppress notification permission prompts in this tab, regardless of the
  // origin requesting the permission.
  bool is_notification_prompt_cooldown_active_ = false;

  // Decides if the quiet prompt UI should be used to display notification
  // permission requests.
  std::unique_ptr<NotificationPermissionUiSelector>
      notification_permission_ui_selector_;

  // Whether the view for the current |requests_| has been shown to the user at
  // least once.
  bool current_request_already_displayed_ = false;

  // Whether to use the normal or quiet UI to display the current permission
  // |requests_|, and whether to show warnings. This will be nullopt if we are
  // still waiting on the result from |notification_permission_ui_selector_|.
  base::Optional<UiDecision> current_request_ui_to_use_;

  // Whether the bubble is being destroyed by this class, rather than in
  // response to a UI event. In this case, callbacks from the bubble itself
  // should be ignored.
  bool deleting_bubble_ = false;

  // Whether the web contents associated with this request manager supports
  // permission prompts.
  bool web_contents_supports_permission_requests_ = true;

  base::WeakPtrFactory<PermissionRequestManager> weak_factory_{this};
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_PERMISSION_REQUEST_MANAGER_H_
