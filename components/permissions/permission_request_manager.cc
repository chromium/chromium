// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_request_manager.h"

#include <algorithm>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/circular_deque.h"
#include "base/feature_list.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"
#include "components/autofill_assistant/browser/public/runtime_manager.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "components/permissions/permission_prompt.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_request_id.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permissions_client.h"
#include "components/permissions/switches.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "url/origin.h"

namespace permissions {

const char kAbusiveNotificationRequestsEnforcementMessage[] =
    "Chrome is blocking notification permission requests on this site because "
    "the site tends to show permission requests that mislead, trick, or force "
    "users into allowing notifications. You should fix the issues as soon as "
    "possible and submit your site for another review. Learn more at "
    "https://support.google.com/webtools/answer/9799048.";

const char kAbusiveNotificationRequestsWarningMessage[] =
    "Chrome might start blocking notification permission requests on this site "
    "in the future because the site tends to show permission requests that "
    "mislead, trick, or force users into allowing notifications. You should "
    "fix the issues as soon as possible and submit your site for another "
    "review. Learn more at https://support.google.com/webtools/answer/9799048.";

constexpr char kAbusiveNotificationContentEnforcementMessage[] =
    "Chrome is blocking notification permission requests on this site because "
    "the site tends to show notifications with content that mislead or trick "
    "users. You should fix the issues as soon as possible and submit your site "
    "for another review. Learn more at "
    "https://support.google.com/webtools/answer/9799048";

constexpr char kAbusiveNotificationContentWarningMessage[] =
    "Chrome might start blocking notification permission requests on this site "
    "in the future because the site tends to show notifications with content "
    "that mislead or trick users. You should fix the issues as soon as "
    "possible and submit your site for another review. Learn more at "
    "https://support.google.com/webtools/answer/9799048";

namespace {

bool IsMessageTextEqual(PermissionRequest* a, PermissionRequest* b) {
  if (a == b)
    return true;
  if (a->GetMessageTextFragment() == b->GetMessageTextFragment() &&
      a->GetOrigin() == b->GetOrigin()) {
    return true;
  }
  return false;
}

bool isMediaRequest(PermissionRequestType type) {
  return type == PermissionRequestType::PERMISSION_MEDIASTREAM_MIC ||
         type == PermissionRequestType::PERMISSION_MEDIASTREAM_CAMERA ||
         type == PermissionRequestType::PERMISSION_CAMERA_PAN_TILT_ZOOM;
}

bool isArOrCameraRequest(PermissionRequestType type) {
  return type == PermissionRequestType::PERMISSION_AR ||
         type == PermissionRequestType::PERMISSION_MEDIASTREAM_CAMERA;
}

bool ShouldGroupRequests(PermissionRequest* a, PermissionRequest* b) {
  if (a->GetOrigin() != b->GetOrigin())
    return false;

  // Group if both requests are media requests.
  if (isMediaRequest(a->GetPermissionRequestType()) &&
      isMediaRequest(b->GetPermissionRequestType())) {
    return true;
  }

  // Group if the requests are an AR and a Camera Access request.
  if (isArOrCameraRequest(a->GetPermissionRequestType()) &&
      isArOrCameraRequest(b->GetPermissionRequestType())) {
    return true;
  }

  return false;
}

}  // namespace

// PermissionRequestManager ----------------------------------------------------

bool PermissionRequestManager::RequestAndSource::
    IsSourceFrameInactiveAndDisallowReactivation() const {
  content::RenderFrameHost* rfh =
      content::RenderFrameHost::FromID(render_process_id, render_frame_id);
  return !rfh || rfh->IsInactiveAndDisallowReactivation();
}

PermissionRequestManager::~PermissionRequestManager() {
  DCHECK(!IsRequestInProgress());
  DCHECK(duplicate_requests_.empty());
  DCHECK(queued_requests_.empty());
}

void PermissionRequestManager::AddRequest(
    content::RenderFrameHost* source_frame,
    PermissionRequest* request) {
  DCHECK(source_frame);
  DCHECK_EQ(content::WebContents::FromRenderFrameHost(source_frame),
            web_contents());

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDenyPermissionPrompts)) {
    request->PermissionDenied();
    request->RequestFinished();
    return;
  }

  if (is_notification_prompt_cooldown_active_ &&
      request->GetContentSettingsType() == ContentSettingsType::NOTIFICATIONS) {
    // Short-circuit by canceling rather than denying to avoid creating a large
    // number of content setting exceptions on Desktop / disabled notification
    // channels on Android.
    request->Cancelled();
    request->RequestFinished();
    return;
  }

  if (!web_contents_supports_permission_requests_) {
    request->Cancelled();
    request->RequestFinished();
    return;
  }

  // TODO(tsergeant): change the UMA to no longer mention bubbles.
  base::RecordAction(base::UserMetricsAction("PermissionBubbleRequest"));

  // TODO(gbillock): is there a race between an early request on a
  // newly-navigated page and the to-be-cleaned-up requests on the previous
  // page? We should maybe listen to DidStartNavigationToPendingEntry (and
  // any other renderer-side nav initiations?). Double-check this for
  // correct behavior on interstitials -- we probably want to basically queue
  // any request for which GetVisibleURL != GetLastCommittedURL.
  const GURL& main_frame_url_ = web_contents()->GetLastCommittedURL();
  bool is_main_frame =
      url::Origin::Create(main_frame_url_)
          .IsSameOriginWith(url::Origin::Create(request->GetOrigin()));

  base::Optional<url::Origin> auto_approval_origin =
      PermissionsClient::Get()->GetAutoApprovalOrigin();
  if (auto_approval_origin) {
    if (url::Origin::Create(request->GetOrigin()) ==
        auto_approval_origin.value()) {
      request->PermissionGranted();
    }
    request->RequestFinished();
    return;
  }

  // Cancel permission requests wile Autofill Assistant's UI is shown.
  auto* assistant_runtime_manager =
      autofill_assistant::RuntimeManager::GetForWebContents(web_contents());
  if (assistant_runtime_manager && assistant_runtime_manager->GetState() ==
                                       autofill_assistant::UIState::kShown) {
    request->Cancelled();
    request->RequestFinished();
    return;
  }

  // Don't re-add an existing request or one with a duplicate text request.
  PermissionRequest* existing_request = GetExistingRequest(request);
  if (existing_request) {
    // |request| is a duplicate. Add it to |duplicate_requests_| unless it's the
    // same object as |existing_request| or an existing duplicate.
    if (request == existing_request)
      return;
    auto range = duplicate_requests_.equal_range(existing_request);
    for (auto it = range.first; it != range.second; ++it) {
      if (request == it->second)
        return;
    }
    duplicate_requests_.insert(std::make_pair(existing_request, request));
    return;
  }

  if (is_main_frame) {
    if (IsRequestInProgress()) {
      base::RecordAction(
          base::UserMetricsAction("PermissionBubbleRequestQueued"));
    }
  } else {
    base::RecordAction(
        base::UserMetricsAction("PermissionBubbleIFrameRequestQueued"));
  }
  queued_requests_.push_back({source_frame->GetProcess()->GetID(),
                              source_frame->GetRoutingID(), request});

  // If we're displaying a quiet permission request, kill it in favor of this
  // permission request.
  if (ShouldCurrentRequestUseQuietUI()) {
    // FinalizeBubble will call ScheduleDequeueRequest on its own.
    FinalizeBubble(PermissionAction::IGNORED);
  } else {
    ScheduleDequeueRequestIfNeeded();
  }
}

void PermissionRequestManager::UpdateAnchorPosition() {
  if (view_)
    view_->UpdateAnchorPosition();
}

void PermissionRequestManager::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  // Cooldown lasts until the next user-initiated navigation, which is defined
  // as either a renderer-initiated navigation with a user gesture, or a
  // browser-initiated navigation.
  //
  // TODO(crbug.com/952347): This check has to be done at DidStartNavigation
  // time, the HasUserGesture state is lost by the time the navigation commits.
  if (!navigation_handle->IsRendererInitiated() ||
      navigation_handle->HasUserGesture()) {
    is_notification_prompt_cooldown_active_ = false;
  }
}

void PermissionRequestManager::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() ||
      !navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  if (!queued_requests_.empty() || IsRequestInProgress()) {
    // |queued_requests_| and |requests_| will be deleted below, which
    // might be a problem for back-forward cache — the page might be restored
    // later, but the requests won't be.
    // Disable bfcache here if we have any requests here to prevent this
    // from happening.
    web_contents()
        ->GetController()
        .GetBackForwardCache()
        .DisableForRenderFrameHost(
            navigation_handle->GetPreviousRenderFrameHostId(),
            "PermissionRequestManager");
  }

  CleanUpRequests();
}

void PermissionRequestManager::DocumentOnLoadCompletedInMainFrame() {
  // This is scheduled because while all calls to the browser have been
  // issued at DOMContentLoaded, they may be bouncing around in scheduled
  // callbacks finding the UI thread still. This makes sure we allow those
  // scheduled calls to AddRequest to complete before we show the page-load
  // permissions bubble.
  ScheduleDequeueRequestIfNeeded();
}

void PermissionRequestManager::DOMContentLoaded(
    content::RenderFrameHost* render_frame_host) {
  ScheduleDequeueRequestIfNeeded();
}

void PermissionRequestManager::WebContentsDestroyed() {
  // If the web contents has been destroyed, treat the bubble as cancelled.
  CleanUpRequests();

  // The WebContents is going away; be aggressively paranoid and delete
  // ourselves lest other parts of the system attempt to add permission bubbles
  // or use us otherwise during the destruction.
  web_contents()->RemoveUserData(UserDataKey());
  // That was the equivalent of "delete this". This object is now destroyed;
  // returning from this function is the only safe thing to do.
}

void PermissionRequestManager::OnVisibilityChanged(
    content::Visibility visibility) {
  bool tab_was_hidden = tab_is_hidden_;
  tab_is_hidden_ = visibility == content::Visibility::HIDDEN;
  if (tab_was_hidden == tab_is_hidden_)
    return;

  if (tab_is_hidden_) {
    if (view_) {
      switch (view_->GetTabSwitchingBehavior()) {
        case PermissionPrompt::TabSwitchingBehavior::
            kDestroyPromptButKeepRequestPending:
          DeleteBubble();
          break;
        case PermissionPrompt::TabSwitchingBehavior::
            kDestroyPromptAndIgnoreRequest:
          FinalizeBubble(PermissionAction::IGNORED);
          break;
        case PermissionPrompt::TabSwitchingBehavior::kKeepPromptAlive:
          break;
      }
    }

    return;
  }

  if (!web_contents()->IsDocumentOnLoadCompletedInMainFrame())
    return;

  if (!IsRequestInProgress()) {
    ScheduleDequeueRequestIfNeeded();
    return;
  }

  if (view_) {
    // We switched tabs away and back while a prompt was active.
    DCHECK_EQ(view_->GetTabSwitchingBehavior(),
              PermissionPrompt::TabSwitchingBehavior::kKeepPromptAlive);
  } else if (current_request_ui_to_use_.has_value()) {
    ShowBubble();
  }
}

const std::vector<PermissionRequest*>& PermissionRequestManager::Requests() {
  return requests_;
}

GURL PermissionRequestManager::GetEmbeddingOrigin() const {
  return web_contents()->GetLastCommittedURL().GetOrigin();
}

void PermissionRequestManager::Accept() {
  if (deleting_bubble_)
    return;
  DCHECK(view_);
  std::vector<PermissionRequest*>::iterator requests_iter;
  for (requests_iter = requests_.begin(); requests_iter != requests_.end();
       requests_iter++) {
    PermissionGrantedIncludingDuplicates(*requests_iter);
  }
  FinalizeBubble(PermissionAction::GRANTED);
}

void PermissionRequestManager::Deny() {
  if (deleting_bubble_)
    return;
  DCHECK(view_);

  // Suppress any further prompts in this WebContents, from any origin, until
  // there is a user-initiated navigation. This stops users from getting trapped
  // in request loops where the website automatically navigates cross-origin
  // (e.g. to another subdomain) to be able to prompt again after a rejection.
  if (base::FeatureList::IsEnabled(
          features::kBlockRepeatedNotificationPermissionPrompts) &&
      std::any_of(requests_.begin(), requests_.end(), [](const auto* request) {
        return request->GetContentSettingsType() ==
               ContentSettingsType::NOTIFICATIONS;
      })) {
    is_notification_prompt_cooldown_active_ = true;
  }

  std::vector<PermissionRequest*>::iterator requests_iter;
  for (requests_iter = requests_.begin(); requests_iter != requests_.end();
       requests_iter++) {
    PermissionDeniedIncludingDuplicates(*requests_iter);
  }
  FinalizeBubble(PermissionAction::DENIED);
}

void PermissionRequestManager::Closing() {
  if (deleting_bubble_)
    return;
  DCHECK(view_);
  std::vector<PermissionRequest*>::iterator requests_iter;
  for (requests_iter = requests_.begin(); requests_iter != requests_.end();
       requests_iter++) {
    CancelledIncludingDuplicates(*requests_iter);
  }
  FinalizeBubble(PermissionAction::DISMISSED);
}

bool PermissionRequestManager::WasCurrentRequestAlreadyDisplayed() {
  return current_request_already_displayed_;
}

PermissionRequestManager::PermissionRequestManager(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      view_factory_(base::Bind(&PermissionPrompt::Create)),
      view_(nullptr),
      tab_is_hidden_(web_contents->GetVisibility() ==
                     content::Visibility::HIDDEN),
      auto_response_for_test_(NONE),
      notification_permission_ui_selector_(
          PermissionsClient::Get()->CreateNotificationPermissionUiSelector(
              web_contents->GetBrowserContext())) {}

void PermissionRequestManager::ScheduleShowBubble() {
  base::RecordAction(base::UserMetricsAction("PermissionBubbleRequest"));
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&PermissionRequestManager::ShowBubble,
                                weak_factory_.GetWeakPtr()));
}

void PermissionRequestManager::DequeueRequestIfNeeded() {
  if (!web_contents()->IsDocumentOnLoadCompletedInMainFrame() || view_ ||
      IsRequestInProgress()) {
    return;
  }

  // Find first valid request.
  while (!queued_requests_.empty()) {
    RequestAndSource& front = queued_requests_.front();

    if (!front.IsSourceFrameInactiveAndDisallowReactivation()) {
      requests_.push_back(front.request);
      queued_requests_.pop_front();
      break;
    }
    front.request->Cancelled();
    front.request->RequestFinished();
    queued_requests_.pop_front();
  }

  if (requests_.empty()) {
    return;
  }

  // Find additional requests that can be grouped with the first one.
  for (; !queued_requests_.empty(); queued_requests_.pop_front()) {
    RequestAndSource& front = queued_requests_.front();
    if (front.IsSourceFrameInactiveAndDisallowReactivation()) {
      front.request->Cancelled();
      front.request->RequestFinished();
    } else if (ShouldGroupRequests(requests_.front(), front.request)) {
      requests_.push_back(front.request);
    } else {
      break;
    }
  }

  if (notification_permission_ui_selector_ &&
      requests_.front()->GetPermissionRequestType() ==
          PermissionRequestType::PERMISSION_NOTIFICATIONS) {
    notification_permission_ui_selector_->SelectUiToUse(
        requests_.front(),
        base::BindOnce(
            &PermissionRequestManager::OnSelectedUiToUseForNotifications,
            weak_factory_.GetWeakPtr()));
  } else {
    current_request_ui_to_use_ =
        UiDecision(UiDecision::UseNormalUi(), UiDecision::ShowNoWarning());
    ScheduleShowBubble();
  }
}

void PermissionRequestManager::ScheduleDequeueRequestIfNeeded() {
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&PermissionRequestManager::DequeueRequestIfNeeded,
                     weak_factory_.GetWeakPtr()));
}

void PermissionRequestManager::ShowBubble() {
  // There is a race condition where the request might have been removed already
  // so double-checking that there is a request in progress (crbug.com/1041222).
  if (!IsRequestInProgress())
    return;

  DCHECK(!view_);
  DCHECK(web_contents()->IsDocumentOnLoadCompletedInMainFrame());
  DCHECK(current_request_ui_to_use_);

  if (tab_is_hidden_)
    return;

  view_ = view_factory_.Run(web_contents(), this);
  if (!view_)
    return;

  if (!current_request_already_displayed_) {
    PermissionUmaUtil::PermissionPromptShown(requests_);

    if (ShouldCurrentRequestUseQuietUI()) {
      switch (ReasonForUsingQuietUi()) {
        case QuietUiReason::kEnabledInPrefs:
        case QuietUiReason::kTriggeredByCrowdDeny:
          break;
        case QuietUiReason::kTriggeredDueToAbusiveRequests:
          LogWarningToConsole(kAbusiveNotificationRequestsEnforcementMessage);
          break;
        case QuietUiReason::kTriggeredDueToAbusiveContent:
          LogWarningToConsole(kAbusiveNotificationContentEnforcementMessage);
          break;
      }
      base::RecordAction(base::UserMetricsAction(
          "Notifications.Quiet.PermissionRequestShown"));
    }

    if (current_request_ui_to_use_->warning_reason) {
      switch (*(current_request_ui_to_use_->warning_reason)) {
        case WarningReason::kAbusiveRequests:
          LogWarningToConsole(kAbusiveNotificationRequestsWarningMessage);
          break;
        case WarningReason::kAbusiveContent:
          LogWarningToConsole(kAbusiveNotificationContentWarningMessage);
          break;
      }
    }
  }
  current_request_already_displayed_ = true;
  NotifyBubbleAdded();

  // If in testing mode, automatically respond to the bubble that was shown.
  if (auto_response_for_test_ != NONE)
    DoAutoResponseForTesting();
}

void PermissionRequestManager::DeleteBubble() {
  DCHECK(view_);
  {
    base::AutoReset<bool> deleting(&deleting_bubble_, true);
    view_.reset();
  }
  NotifyBubbleRemoved();
}

void PermissionRequestManager::FinalizeBubble(
    PermissionAction permission_action) {
  DCHECK(IsRequestInProgress());

  PermissionUmaUtil::PermissionPromptResolved(
      requests_, web_contents(), permission_action,
      DetermineCurrentRequestUIDispositionForUMA());

  content::BrowserContext* browser_context =
      web_contents()->GetBrowserContext();
  PermissionDecisionAutoBlocker* autoblocker =
      PermissionsClient::Get()->GetPermissionDecisionAutoBlocker(
          browser_context);

  base::Optional<QuietUiReason> quiet_ui_reason;
  if (ShouldCurrentRequestUseQuietUI())
    quiet_ui_reason = ReasonForUsingQuietUi();

  for (PermissionRequest* request : requests_) {
    // TODO(timloh): We only support dismiss and ignore embargo for permissions
    // which use PermissionRequestImpl as the other subclasses don't support
    // GetContentSettingsType.
    if (request->GetContentSettingsType() == ContentSettingsType::DEFAULT)
      continue;

    PermissionsClient::Get()->OnPromptResolved(
        browser_context, request->GetPermissionRequestType(), permission_action,
        request->GetOrigin(), quiet_ui_reason);

    PermissionEmbargoStatus embargo_status =
        PermissionEmbargoStatus::NOT_EMBARGOED;
    if (permission_action == PermissionAction::DISMISSED) {
      if (autoblocker->RecordDismissAndEmbargo(
              request->GetOrigin(), request->GetContentSettingsType(),
              ShouldCurrentRequestUseQuietUI())) {
        embargo_status = PermissionEmbargoStatus::REPEATED_DISMISSALS;
      }
    } else if (permission_action == PermissionAction::IGNORED) {
      if (autoblocker->RecordIgnoreAndEmbargo(
              request->GetOrigin(), request->GetContentSettingsType(),
              ShouldCurrentRequestUseQuietUI())) {
        embargo_status = PermissionEmbargoStatus::REPEATED_IGNORES;
      }
    }
    PermissionUmaUtil::RecordEmbargoStatus(embargo_status);
  }
  std::vector<PermissionRequest*>::iterator requests_iter;
  for (requests_iter = requests_.begin(); requests_iter != requests_.end();
       requests_iter++) {
    RequestFinishedIncludingDuplicates(*requests_iter);
  }
  requests_.clear();

  if (notification_permission_ui_selector_)
    notification_permission_ui_selector_->Cancel();

  current_request_already_displayed_ = false;
  current_request_ui_to_use_.reset();

  if (view_)
    DeleteBubble();

  ScheduleDequeueRequestIfNeeded();
}

void PermissionRequestManager::CleanUpRequests() {
  for (auto& queued_request : queued_requests_) {
    CancelledIncludingDuplicates(queued_request.request);
    RequestFinishedIncludingDuplicates(queued_request.request);
  }
  queued_requests_.clear();

  if (IsRequestInProgress()) {
    std::vector<PermissionRequest*>::iterator requests_iter;
    for (requests_iter = requests_.begin(); requests_iter != requests_.end();
         requests_iter++) {
      CancelledIncludingDuplicates(*requests_iter);
    }
    FinalizeBubble(PermissionAction::IGNORED);
  }
}

PermissionRequest* PermissionRequestManager::GetExistingRequest(
    PermissionRequest* request) {
  for (PermissionRequest* existing_request : requests_) {
    if (IsMessageTextEqual(existing_request, request))
      return existing_request;
  }
  for (RequestAndSource& request_and_source : queued_requests_) {
    if (IsMessageTextEqual(request_and_source.request, request))
      return request_and_source.request;
  }
  return nullptr;
}

void PermissionRequestManager::PermissionGrantedIncludingDuplicates(
    PermissionRequest* request) {
  DCHECK_EQ(1, base::STLCount(requests_, request) +
                   CountQueuedPermissionRequests(request))
      << "Only requests in [queued_[frame_]]requests_ can have duplicates";
  request->PermissionGranted();
  auto range = duplicate_requests_.equal_range(request);
  for (auto it = range.first; it != range.second; ++it)
    it->second->PermissionGranted();
}

void PermissionRequestManager::PermissionDeniedIncludingDuplicates(
    PermissionRequest* request) {
  DCHECK_EQ(1, base::STLCount(requests_, request) +
                   CountQueuedPermissionRequests(request))
      << "Only requests in [queued_]requests_ can have duplicates";
  request->PermissionDenied();
  auto range = duplicate_requests_.equal_range(request);
  for (auto it = range.first; it != range.second; ++it)
    it->second->PermissionDenied();
}

void PermissionRequestManager::CancelledIncludingDuplicates(
    PermissionRequest* request) {
  DCHECK_EQ(1, base::STLCount(requests_, request) +
                   CountQueuedPermissionRequests(request))
      << "Only requests in [queued_]requests_ can have duplicates";
  request->Cancelled();
  auto range = duplicate_requests_.equal_range(request);
  for (auto it = range.first; it != range.second; ++it)
    it->second->Cancelled();
}

void PermissionRequestManager::RequestFinishedIncludingDuplicates(
    PermissionRequest* request) {
  DCHECK_EQ(1, base::STLCount(requests_, request) +
                   CountQueuedPermissionRequests(request))
      << "Only requests in [queued_]requests_ can have duplicates";
  request->RequestFinished();
  // Beyond this point, |request| has probably been deleted.
  auto range = duplicate_requests_.equal_range(request);
  for (auto it = range.first; it != range.second; ++it)
    it->second->RequestFinished();
  // Additionally, we can now remove the duplicates.
  duplicate_requests_.erase(request);
}

void PermissionRequestManager::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void PermissionRequestManager::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

bool PermissionRequestManager::ShouldCurrentRequestUseQuietUI() const {
  if (!IsRequestInProgress())
    return false;

  // ContentSettingImageModel might call into this method if the user switches
  // between tabs while the |notification_permission_ui_selector_| is pending.
  return current_request_ui_to_use_ &&
         current_request_ui_to_use_->quiet_ui_reason;
}

PermissionRequestManager::QuietUiReason
PermissionRequestManager::ReasonForUsingQuietUi() const {
  return *(current_request_ui_to_use_->quiet_ui_reason);
}

bool PermissionRequestManager::IsRequestInProgress() const {
  return !requests_.empty();
}

void PermissionRequestManager::NotifyBubbleAdded() {
  for (Observer& observer : observer_list_)
    observer.OnBubbleAdded();
}

void PermissionRequestManager::NotifyBubbleRemoved() {
  for (Observer& observer : observer_list_)
    observer.OnBubbleRemoved();
}

void PermissionRequestManager::OnSelectedUiToUseForNotifications(
    const UiDecision& decision) {
  current_request_ui_to_use_ = decision;
  ScheduleShowBubble();
}

PermissionPromptDisposition
PermissionRequestManager::DetermineCurrentRequestUIDispositionForUMA() {
  if (view_)
    return view_->GetPromptDisposition();
  return PermissionPromptDisposition::NONE_VISIBLE;
}

void PermissionRequestManager::LogWarningToConsole(const char* message) {
  web_contents()->GetMainFrame()->AddMessageToConsole(
      blink::mojom::ConsoleMessageLevel::kWarning, message);
}

void PermissionRequestManager::DoAutoResponseForTesting() {
  switch (auto_response_for_test_) {
    case ACCEPT_ALL:
      Accept();
      break;
    case DENY_ALL:
      Deny();
      break;
    case DISMISS:
      Closing();
      break;
    case NONE:
      NOTREACHED();
  }
}

int PermissionRequestManager::CountQueuedPermissionRequests(
    PermissionRequest* request) {
  return std::count_if(queued_requests_.begin(), queued_requests_.end(),
                       [request](const RequestAndSource& entry) {
                         return request == entry.request;
                       });
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PermissionRequestManager)

}  // namespace permissions
