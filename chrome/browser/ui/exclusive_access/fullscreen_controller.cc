// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"

#include "base/auto_reset.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/ui/blocked_content/popunder_preventer.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_within_tab_helper.h"
#include "chrome/browser/ui/status_bubble.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "content/public/browser/fullscreen_types.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"

#if !BUILDFLAG(IS_MAC)
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#endif

using content::WebContents;

namespace {

constexpr char kHistogramFullscreenWebsiteStateAtApiRequest[] =
    "WebCore.Fullscreen.WebsiteStateAtApiRequest";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class WebsiteStateAtFullscreenRequest {
  kNotAllowlistedNotVisited = 0,
  kNotAllowlistedVisited = 1,
  kNotAllowlistedVisitStateUnknown = 2,
  kAllowlistedNotVisited = 3,
  kAllowlistedVisited = 4,
  kAllowlistedVisitStateUnknown = 5,
  kAllowlistStateUnknownNotVisited = 6,
  kAllowlistStateUnknownVisited = 7,
  kAllowlistStateUnknownVisitStateUnknown = 8,
  kMaxValue = kAllowlistStateUnknownVisitStateUnknown,
};

bool IsAnotherScreen(const WebContents& web_contents,
                     const int64_t display_id) {
  if (display_id == display::kInvalidDisplayId)
    return false;
  return display_id != FullscreenController::GetDisplayId(web_contents);
}

void RecordWebsiteStateAtApiRequest(history::HistoryLastVisitResult result,
                                    std::optional<bool> on_allowlist) {
  auto state = WebsiteStateAtFullscreenRequest::kNotAllowlistedNotVisited;
  if (!result.success) {
    if (!on_allowlist.has_value()) {
      state = WebsiteStateAtFullscreenRequest::
          kAllowlistStateUnknownVisitStateUnknown;
    } else if (*on_allowlist) {
      state = WebsiteStateAtFullscreenRequest::kAllowlistedVisitStateUnknown;
    } else {
      state = WebsiteStateAtFullscreenRequest::kNotAllowlistedVisitStateUnknown;
    }
  } else if (!result.last_visit.is_null()) {
    if (!on_allowlist.has_value()) {
      state = WebsiteStateAtFullscreenRequest::kAllowlistStateUnknownVisited;
    } else if (*on_allowlist) {
      state = WebsiteStateAtFullscreenRequest::kAllowlistedVisited;
    } else {
      state = WebsiteStateAtFullscreenRequest::kNotAllowlistedVisited;
    }
  } else if (!on_allowlist.has_value()) {
    state = WebsiteStateAtFullscreenRequest::kAllowlistStateUnknownNotVisited;
  } else if (*on_allowlist) {
    state = WebsiteStateAtFullscreenRequest::kAllowlistedNotVisited;
  }
  base::UmaHistogramEnumeration(kHistogramFullscreenWebsiteStateAtApiRequest,
                                state);
}

void CheckUrlForAllowlistAndRecordMetric(
    const GURL& url,
    history::HistoryLastVisitResult result) {
  if (!g_browser_process->safe_browsing_service() ||
      !g_browser_process->safe_browsing_service()->database_manager()) {
    RecordWebsiteStateAtApiRequest(result, std::nullopt);
    return;
  }
  g_browser_process->safe_browsing_service()
      ->database_manager()
      ->CheckUrlForHighConfidenceAllowlist(
          url,
          base::BindOnce(
              [](history::HistoryLastVisitResult result, bool on_allowlist,
                 std::optional<safe_browsing::SafeBrowsingDatabaseManager::
                                   HighConfidenceAllowlistCheckLoggingDetails>
                     logging_details) {
                RecordWebsiteStateAtApiRequest(result, on_allowlist);
              },
              result));
}

}  // namespace

FullscreenController::FullscreenController(ExclusiveAccessManager* manager)
    : ExclusiveAccessControllerBase(manager) {}

FullscreenController::~FullscreenController() = default;

void FullscreenController::AddObserver(FullscreenObserver* observer) {
  observer_list_.AddObserver(observer);
}

void FullscreenController::RemoveObserver(FullscreenObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

int64_t FullscreenController::GetDisplayId(const WebContents& web_contents) {
  if (auto* screen = display::Screen::GetScreen()) {
    // crbug.com/1347558 WebContents::GetNativeView is const-incorrect.
    // const_cast is used to access GetNativeView(). Also GetDisplayNearestView
    // should accept const gfx::NativeView, but there is other const
    // incorrectness down the call chain in some implementations.
    auto display = screen->GetDisplayNearestView(
        const_cast<WebContents&>(web_contents).GetNativeView());
    return display.id();
  }
  return display::kInvalidDisplayId;
}

bool FullscreenController::IsFullscreenForBrowser() const {
  return exclusive_access_manager()->context()->IsFullscreen() &&
         !IsFullscreenCausedByTab();
}

void FullscreenController::ToggleBrowserFullscreenMode() {
  extension_caused_fullscreen_ = GURL();
  ToggleFullscreenModeInternal(BROWSER, nullptr, display::kInvalidDisplayId);
}

void FullscreenController::ToggleBrowserFullscreenModeWithExtension(
    const GURL& extension_url) {
  // |extension_caused_fullscreen_| will be reset if this causes fullscreen to
  // exit.
  extension_caused_fullscreen_ = extension_url;
  ToggleFullscreenModeInternal(BROWSER, nullptr, display::kInvalidDisplayId);
}

bool FullscreenController::IsWindowFullscreenForTabOrPending() const {
  return exclusive_access_tab() || is_tab_fullscreen_for_testing_;
}

bool FullscreenController::IsExtensionFullscreenOrPending() const {
  return !extension_caused_fullscreen_.is_empty();
}

bool FullscreenController::IsControllerInitiatedFullscreen() const {
  return toggled_into_fullscreen_;
}

bool FullscreenController::IsTabFullscreen() const {
  return tab_fullscreen_ || is_tab_fullscreen_for_testing_;
}

content::FullscreenState FullscreenController::GetFullscreenState(
    const content::WebContents* web_contents) const {
  content::FullscreenState state;
  CHECK(web_contents) << "Null web_contents passed to GetFullscreenState";

  // Handle screen-captured tab fullscreen and `is_tab_fullscreen_for_testing_`.
  if (IsFullscreenWithinTab(web_contents)) {
    state.target_mode = content::FullscreenMode::kPseudoContent;
    return state;
  }

  // Handle not fullscreen, browser fullscreen, and exiting tab fullscreen.
  if (!tab_fullscreen_ || web_contents != exclusive_access_tab()) {
    state.target_mode = content::FullscreenMode::kWindowed;
    return state;
  }

  // Handle tab fullscreen and entering tab fullscreen.
  state.target_mode = content::FullscreenMode::kContent;
  state.target_display_id =
      (tab_fullscreen_target_display_id_ != display::kInvalidDisplayId)
          ? tab_fullscreen_target_display_id_
          : GetDisplayId(*web_contents);
  return state;
}

bool FullscreenController::IsFullscreenCausedByTab() const {
  return state_prior_to_tab_fullscreen_ == STATE_NORMAL;
}

bool FullscreenController::CanEnterFullscreenModeForTab(
    content::RenderFrameHost* requesting_frame) {
  DCHECK(requesting_frame);
  auto* web_contents = WebContents::FromRenderFrameHost(requesting_frame);
  DCHECK(web_contents);

  if (web_contents != exclusive_access_manager()
                          ->context()
                          ->GetWebContentsForExclusiveAccess()) {
    return false;
  }

  return true;
}

void FullscreenController::EnterFullscreenModeForTab(
    content::RenderFrameHost* requesting_frame,
    const int64_t display_id) {
  RecordMetricsOnFullscreenApiRequested(requesting_frame);
  DCHECK(requesting_frame);
  // This function should never fail. Any possible failures must be checked in
  // |CanEnterFullscreenModeForTab()| instead. Silently dropping the request
  // could cause requestFullscreen promises to hang. If we are in this function,
  // the renderer expects a visual property update to call
  // |blink::FullscreenController::DidEnterFullscreen| to resolve promises.
  DCHECK(CanEnterFullscreenModeForTab(requesting_frame));
  auto* web_contents = WebContents::FromRenderFrameHost(requesting_frame);
  DCHECK(web_contents);

  if (MaybeToggleFullscreenWithinTab(web_contents, true)) {
    // During tab capture of fullscreen-within-tab views, the browser window
    // fullscreen state is unchanged, so return now.
    return;
  }

  if (!popunder_preventer_)
    popunder_preventer_ = std::make_unique<PopunderPreventer>(web_contents);
  else
    popunder_preventer_->WillActivateWebContents(web_contents);

  // Keep the current state. |SetTabWithExclusiveAccess| may change the return
  // value of |IsWindowFullscreenForTabOrPending|.
  const bool requesting_another_screen =
      IsAnotherScreen(*web_contents, display_id);
  const bool was_window_fullscreen_for_tab_or_pending =
      !requesting_another_screen && IsWindowFullscreenForTabOrPending();

  if (exclusive_access_tab() && exclusive_access_tab() != web_contents) {
    // This unexpected condition may be hit in practice; see crbug.com/1456875.
    // In known circumstances it is safe to just clear the exclusive_access_tab,
    // but behavior and assumptions should be rectified; see crbug.com/1244121.
    NOTIMPLEMENTED() << "Conflicting exclusive access tab assignment detected";
    SetTabWithExclusiveAccess(nullptr);
  }
  SetTabWithExclusiveAccess(web_contents);
  requesting_origin_ =
      requesting_frame->GetLastCommittedURL().DeprecatedGetOriginAsURL();

  if (was_window_fullscreen_for_tab_or_pending) {
    // While an element is in fullscreen, requesting fullscreen for a different
    // element in the tab is handled in the renderer process if both elements
    // are in the same process. But the request will come to the browser when
    // the element is in a different process, such as OOPIF, because the
    // renderer doesn't know if an element in other renderer process is in
    // fullscreen.
    DCHECK(tab_fullscreen_);
  } else {
    ExclusiveAccessContext* exclusive_access_context =
        exclusive_access_manager()->context();
    // This is needed on Mac as entering into Tab Fullscreen might change the
    // top UI style.
    exclusive_access_context->UpdateUIForTabFullscreen();

    state_prior_to_tab_fullscreen_ =
        IsFullscreenForBrowser() ? STATE_BROWSER_FULLSCREEN : STATE_NORMAL;

    if (!exclusive_access_context->IsFullscreen() ||
        requesting_another_screen) {
      EnterFullscreenModeInternal(TAB, requesting_frame, display_id);
      return;
    }

    // We need to update the fullscreen exit bubble, e.g., going from browser
    // fullscreen to tab fullscreen will need to show different content.
    tab_fullscreen_ = true;
    exclusive_access_manager()->UpdateBubble(base::NullCallback());
  }

  // This is only a change between Browser and Tab fullscreen. We generate
  // a fullscreen notification now because there is no window change.
  PostFullscreenChangeNotification();
}

void FullscreenController::ExitFullscreenModeForTab(WebContents* web_contents) {
  if (MaybeToggleFullscreenWithinTab(web_contents, false)) {
    // During tab capture of fullscreen-within-tab views, the browser window
    // fullscreen state is unchanged, so return now.
    return;
  }

  if (!IsWindowFullscreenForTabOrPending() ||
      web_contents != exclusive_access_tab()) {
    return;
  }

  ExclusiveAccessContext* exclusive_access_context =
      exclusive_access_manager()->context();

  if (!exclusive_access_context->IsFullscreen())
    return;

  if (IsFullscreenCausedByTab()) {
    // Tab Fullscreen -> Normal.
    ToggleFullscreenModeInternal(TAB, nullptr, display::kInvalidDisplayId);
    return;
  }

  // Tab Fullscreen -> Browser Fullscreen.
  // Exiting tab fullscreen mode may require updating top UI.
  // All exiting tab fullscreen to non-fullscreen mode cases are handled in
  // BrowserNonClientFrameView::OnFullscreenStateChanged(); but exiting tab
  // fullscreen to browser fullscreen should be handled here.
  const bool was_browser_fullscreen =
      state_prior_to_tab_fullscreen_ == STATE_BROWSER_FULLSCREEN;

  NotifyTabExclusiveAccessLost();
  if (was_browser_fullscreen)
    exclusive_access_context->UpdateUIForTabFullscreen();

  // For Tab Fullscreen -> Browser Fullscreen, enter browser fullscreen on the
  // display that originated the browser fullscreen prior to the tab fullscreen.
  // crbug.com/1313606.
  if (was_browser_fullscreen && web_contents &&
      display_id_prior_to_tab_fullscreen_ != display::kInvalidDisplayId &&
      display_id_prior_to_tab_fullscreen_ != GetDisplayId(*web_contents)) {
    EnterFullscreenModeInternal(BROWSER, nullptr,
                                display_id_prior_to_tab_fullscreen_);
    return;
  }

  // Notify observers now, when reverting from Tab fullscreen to Browser
  // fullscreen on the same display. Exiting fullscreen, or reverting to Browser
  // fullscreen on another display, triggers additional controller logic above,
  // which will notify observers when appropriate.
  PostFullscreenChangeNotification();
}

void FullscreenController::FullscreenTabOpeningPopup(
    content::WebContents* opener,
    content::WebContents* popup) {
  if (!popunder_preventer_) {
    return;
  }

  DCHECK_EQ(exclusive_access_tab(), opener);
  popunder_preventer_->AddPotentialPopunder(popup);
}

void FullscreenController::OnTabDeactivated(
    content::WebContents* web_contents) {
  base::AutoReset<raw_ptr<content::WebContents>> auto_resetter(
      &deactivated_contents_, web_contents);
  ExclusiveAccessControllerBase::OnTabDeactivated(web_contents);
}

void FullscreenController::OnTabDetachedFromView(WebContents* old_contents) {
  if (!IsFullscreenWithinTab(old_contents))
    return;

  // A fullscreen-within-tab view undergoing screen capture has been detached
  // and is no longer visible to the user. Set it to exactly the WebContents'
  // preferred size. See 'FullscreenWithinTab Note'.
  //
  // When the user later selects the tab to show |old_contents| again, UI code
  // elsewhere (e.g., views::WebView) will resize the view to fit within the
  // browser window once again.

  // If the view has been detached from the browser window (e.g., to drag a tab
  // off into a new browser window), return immediately to avoid an unnecessary
  // resize.
  if (!old_contents->GetDelegate())
    return;

  // Do nothing if tab capture ended after toggling fullscreen, or a preferred
  // size was never specified by the capturer.
  if (!old_contents->IsBeingCaptured() ||
      old_contents->GetPreferredSize().IsEmpty()) {
    return;
  }

  old_contents->Resize(gfx::Rect(old_contents->GetPreferredSize()));
}

void FullscreenController::OnTabClosing(WebContents* web_contents) {
  if (IsFullscreenWithinTab(web_contents))
    web_contents->ExitFullscreen(
        /* will_cause_resize */ IsFullscreenCausedByTab());
  else
    ExclusiveAccessControllerBase::OnTabClosing(web_contents);
}

void FullscreenController::WindowFullscreenStateChanged() {
  ExclusiveAccessContext* const exclusive_access_context =
      exclusive_access_manager()->context();
  bool exiting_fullscreen = !exclusive_access_context->IsFullscreen();
  PostFullscreenChangeNotification();
  if (exiting_fullscreen) {
    toggled_into_fullscreen_ = false;
    extension_caused_fullscreen_ = GURL();
    NotifyTabExclusiveAccessLost();
  } else {
    toggled_into_fullscreen_ = true;
    if (!IsRunningInAppMode()) {
      exclusive_access_manager()->UpdateBubble(base::NullCallback(),
                                               /*force_update=*/true);
    }
    if (!fullscreen_start_time_) {
      fullscreen_start_time_ = base::TimeTicks::Now();
    }
    // This must be posted because keyboard lock engages right after entering
    // fullscreen, and we want to record the keyboard/pointer lock state after
    // that.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&FullscreenController::RecordMetricsOnEnteringFullscreen,
                       ptr_factory_.GetWeakPtr()));
  }
}

void FullscreenController::FullscreenTransitionCompleted() {
  if (fullscreen_transition_complete_callback_)
    std::move(fullscreen_transition_complete_callback_).Run();
#if DCHECK_IS_ON()
  if (started_fullscreen_transition_ && IsTabFullscreen()) {
    DCHECK(exclusive_access_tab());
    DCHECK_EQ(tab_fullscreen_target_display_id_,
              GetDisplayId(*exclusive_access_tab()));
  }
#endif  // DCHECK_IS_ON()
  tab_fullscreen_target_display_id_ = display::kInvalidDisplayId;
  started_fullscreen_transition_ = false;
  if (!IsTabFullscreen()) {
    // Activate any popup windows created while content fullscreen, after exit.
    popunder_preventer_.reset();
  }
}

void FullscreenController::RunOrDeferUntilTransitionIsComplete(
    base::OnceClosure callback) {
  if (started_fullscreen_transition_)
    fullscreen_transition_complete_callback_ = std::move(callback);
  else
    std::move(callback).Run();
}

bool FullscreenController::HandleUserPressedEscape() {
  WebContents* const active_web_contents =
      exclusive_access_manager()->context()->GetWebContentsForExclusiveAccess();
  if (IsFullscreenWithinTab(active_web_contents)) {
    active_web_contents->ExitFullscreen(
        /* will_cause_resize */ IsFullscreenCausedByTab());
    return true;
  }

  if (!IsWindowFullscreenForTabOrPending())
    return false;

  ExitExclusiveAccessIfNecessary();
  base::RecordAction(base::UserMetricsAction("ExitFullscreen_Esc"));
  return true;
}

void FullscreenController::HandleUserHeldEscape() {
  if (RequiresPressAndHoldEscToExit()) {
    ExitFullscreenModeInternal();
    base::RecordAction(
        base::UserMetricsAction("ExitFullscreen_PressAndHoldEsc"));
  }
}

void FullscreenController::HandleUserReleasedEscapeEarly() {}

bool FullscreenController::RequiresPressAndHoldEscToExit() const {
  return IsFullscreenForBrowser();
}

void FullscreenController::ExitExclusiveAccessToPreviousState() {
  if (IsWindowFullscreenForTabOrPending())
    ExitFullscreenModeForTab(exclusive_access_tab());
  else if (IsFullscreenForBrowser())
    ExitFullscreenModeInternal();
}

GURL FullscreenController::GetURLForExclusiveAccessBubble() const {
  if (exclusive_access_tab())
    return GetRequestingOrigin();
  return extension_caused_fullscreen_;
}

void FullscreenController::ExitExclusiveAccessIfNecessary() {
  if (IsWindowFullscreenForTabOrPending())
    ExitFullscreenModeForTab(exclusive_access_tab());
  else
    NotifyTabExclusiveAccessLost();
}

void FullscreenController::PostFullscreenChangeNotification() {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&FullscreenController::NotifyFullscreenChange,
                                ptr_factory_.GetWeakPtr()));
}

void FullscreenController::NotifyFullscreenChange() {
  for (auto& observer : observer_list_)
    observer.OnFullscreenStateChanged();
}

void FullscreenController::NotifyTabExclusiveAccessLost() {
  if (exclusive_access_tab()) {
    WebContents* web_contents = exclusive_access_tab();
    SetTabWithExclusiveAccess(nullptr);
    requesting_origin_ = GURL();
    bool will_cause_resize = IsFullscreenCausedByTab();
    state_prior_to_tab_fullscreen_ = STATE_INVALID;
    tab_fullscreen_ = false;
    web_contents->ExitFullscreen(will_cause_resize);
    exclusive_access_manager()->UpdateBubble(base::NullCallback());
  }
}

void FullscreenController::ToggleFullscreenModeInternal(
    FullscreenInternalOption option,
    content::RenderFrameHost* requesting_frame,
    const int64_t display_id) {
  ExclusiveAccessContext* const exclusive_access_context =
      exclusive_access_manager()->context();
  bool enter_fullscreen = !exclusive_access_context->IsFullscreen();

  if (enter_fullscreen)
    EnterFullscreenModeInternal(option, requesting_frame, display_id);
  else
    ExitFullscreenModeInternal();
}

void FullscreenController::EnterFullscreenModeInternal(
    FullscreenInternalOption option,
    content::RenderFrameHost* requesting_frame,
    int64_t display_id) {
#if !BUILDFLAG(IS_MAC)
  // Do not enter fullscreen mode if disallowed by pref. This prevents the user
  // from manually entering fullscreen mode and also disables kiosk mode on
  // desktop platforms.
  if (!exclusive_access_manager()
           ->context()
           ->GetProfile()
           ->GetPrefs()
           ->GetBoolean(prefs::kFullscreenAllowed)) {
    return;
  }
#endif
  started_fullscreen_transition_ = true;
  toggled_into_fullscreen_ = true;
  bool entering_tab_fullscreen = option == TAB && !tab_fullscreen_;
  GURL url;
  if (option == TAB) {
    url = GetRequestingOrigin();
    tab_fullscreen_ = true;
    WebContents* web_contents =
        WebContents::FromRenderFrameHost(requesting_frame);
    // Do not enter tab fullscreen if there is no web contents for the
    // requesting frame (This normally shouldn't happen).
    DCHECK(web_contents);
    if (!web_contents)
      return;
    int64_t current_display = GetDisplayId(*web_contents);
    if (display_id != display::kInvalidDisplayId) {
      // Check, but do not prompt, for permission to request a specific screen.
      // Sites generally need permission to get `display_id` in the first place.
      if (!requesting_frame ||
          requesting_frame->GetBrowserContext()
                  ->GetPermissionController()
                  ->GetPermissionStatusForCurrentDocument(
                      blink::PermissionType::WINDOW_MANAGEMENT,
                      requesting_frame) !=
              blink::mojom::PermissionStatus::GRANTED) {
        display_id = display::kInvalidDisplayId;
      } else if (entering_tab_fullscreen) {
        display_id_prior_to_tab_fullscreen_ = current_display;
      }
    }
    tab_fullscreen_target_display_id_ =
        display_id == display::kInvalidDisplayId ? current_display : display_id;
  } else {
    if (!extension_caused_fullscreen_.is_empty())
      url = extension_caused_fullscreen_;
  }

  fullscreen_start_time_ = base::TimeTicks::Now();
  if (option == BROWSER)
    base::RecordAction(base::UserMetricsAction("ToggleFullscreen"));
  // TODO(scheib): Record metrics for WITH_TOOLBAR, without counting transitions
  // from tab fullscreen out to browser with toolbar.

  exclusive_access_manager()->context()->EnterFullscreen(
      url, exclusive_access_manager()->GetExclusiveAccessExitBubbleType(),
      display_id);

  // WindowFullscreenStateChanged() is called once the window is fullscreen.
}

void FullscreenController::ExitFullscreenModeInternal() {
  // In kiosk mode, we always want to be fullscreen.
  if (IsRunningInAppMode()) {
    return;
  }

  // `fullscreen_start_time_` is null if a fullscreen tab moves to a new window.
  if (fullscreen_start_time_ && exclusive_access_tab()) {
    ukm::SourceId source_id =
        exclusive_access_tab()->GetPrimaryMainFrame()->GetPageUkmSourceId();
    ukm::builders::Fullscreen_Exit(source_id)
        .SetSessionDuration(ukm::GetSemanticBucketMinForDurationTiming(
            (base::TimeTicks::Now() - fullscreen_start_time_.value())
                .InMilliseconds()))
        .Record(ukm::UkmRecorder::Get());
    fullscreen_start_time_.reset();
  }

  toggled_into_fullscreen_ = false;
  started_fullscreen_transition_ = true;
#if BUILDFLAG(IS_MAC)
  // Mac windows report a state change instantly, and so we must also clear
  // state_prior_to_tab_fullscreen_ to match them else other logic using
  // state_prior_to_tab_fullscreen_ will be incorrect.
  NotifyTabExclusiveAccessLost();
#endif
  exclusive_access_manager()->context()->ExitFullscreen();
  extension_caused_fullscreen_ = GURL();
  exclusive_access_manager()->UpdateBubble(base::NullCallback());
}

bool FullscreenController::MaybeToggleFullscreenWithinTab(
    WebContents* web_contents,
    bool enter_fullscreen) {
  if (enter_fullscreen) {
    if (web_contents->IsBeingVisiblyCaptured()) {
      FullscreenWithinTabHelper::CreateForWebContents(web_contents);
      FullscreenWithinTabHelper::FromWebContents(web_contents)
          ->SetIsFullscreenWithinTab(true);
      return true;
    }
  } else {
    if (IsFullscreenWithinTab(web_contents)) {
      FullscreenWithinTabHelper::RemoveForWebContents(web_contents);
      return true;
    }
  }

  return false;
}

bool FullscreenController::IsFullscreenWithinTab(
    const WebContents* web_contents) const {
  if (is_tab_fullscreen_for_testing_)
    return true;

  // Note: On Mac, some of the OnTabXXX() methods get called with a nullptr
  // value
  // for web_contents. Check for that here.
  const FullscreenWithinTabHelper* const helper =
      web_contents ? FullscreenWithinTabHelper::FromWebContents(web_contents)
                   : nullptr;
  if (helper && helper->is_fullscreen_within_tab()) {
    DCHECK_NE(exclusive_access_tab(), web_contents);
    return true;
  }
  return false;
}

GURL FullscreenController::GetRequestingOrigin() const {
  DCHECK(exclusive_access_tab());

  if (!requesting_origin_.is_empty())
    return requesting_origin_;

  return exclusive_access_tab()->GetLastCommittedURL();
}

GURL FullscreenController::GetEmbeddingOrigin() const {
  DCHECK(exclusive_access_tab());

  return exclusive_access_tab()->GetLastCommittedURL();
}

void FullscreenController::RecordMetricsOnFullscreenApiRequested(
    content::RenderFrameHost* requesting_frame) {
  history::HistoryService* service =
      HistoryServiceFactory::GetForProfileWithoutCreating(
          exclusive_access_manager()->context()->GetProfile());
  if (service) {
    // Check if the origin has been visited more than a day ago and whether it's
    // on an allowlist, then record those bits of information in a metric.
    service->GetLastVisitToOrigin(
        url::Origin(requesting_frame->GetLastCommittedOrigin()), base::Time(),
        base::Time::Now() - base::Days(1),
        base::BindOnce(&CheckUrlForAllowlistAndRecordMetric,
                       GURL(requesting_frame->GetLastCommittedURL())),
        &task_tracker_);
  } else {
    // The history is unknown, so just check if the URL is on the allowlist and
    // record that.
    CheckUrlForAllowlistAndRecordMetric(requesting_frame->GetLastCommittedURL(),
                                        history::HistoryLastVisitResult());
  }
}

void FullscreenController::RecordMetricsOnEnteringFullscreen() {
  if (IsFullscreenCausedByTab()) {
    exclusive_access_manager()->RecordLockStateOnEnteringApiFullscreen();
  } else {
    exclusive_access_manager()->RecordLockStateOnEnteringBrowserFullscreen();
  }
}
