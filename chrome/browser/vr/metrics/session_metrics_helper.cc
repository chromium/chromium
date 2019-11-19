// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/metrics/session_metrics_helper.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "components/ukm/content/source_url_recorder.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace vr {

namespace {

const void* const kSessionMetricsHelperDataKey = &kSessionMetricsHelperDataKey;

// minimum duration: 7 seconds for video, no minimum for headset/vr modes
// maximum gap: 7 seconds between videos.  no gap for headset/vr-modes
constexpr base::TimeDelta kMinimumVideoSessionDuration(
    base::TimeDelta::FromSecondsD(7));
constexpr base::TimeDelta kMaximumVideoSessionGap(
    base::TimeDelta::FromSecondsD(7));

constexpr base::TimeDelta kMinimumHeadsetSessionDuration(
    base::TimeDelta::FromSecondsD(0));
constexpr base::TimeDelta kMaximumHeadsetSessionGap(
    base::TimeDelta::FromSecondsD(0));

// Handles the lifetime of the helper which is attached to a WebContents.
class SessionMetricsHelperData : public base::SupportsUserData::Data {
 public:
  explicit SessionMetricsHelperData(
      SessionMetricsHelper* session_metrics_helper)
      : session_metrics_helper_(session_metrics_helper) {}

  ~SessionMetricsHelperData() override { delete session_metrics_helper_; }

  SessionMetricsHelper* get() const { return session_metrics_helper_; }

 private:
  SessionMetricsHelper* session_metrics_helper_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(SessionMetricsHelperData);
};

device::SessionMode ConvertRuntimeOptionsToSessionMode(
    const device::mojom::XRRuntimeSessionOptions& options) {
  if (!options.immersive)
    return device::SessionMode::kInline;

  if (options.environment_integration)
    return device::SessionMode::kImmersiveAr;

  return device::SessionMode::kImmersiveVr;
}

}  // namespace

WebXRSessionTracker::WebXRSessionTracker(
    std::unique_ptr<ukm::builders::XR_WebXR_Session> entry)
    : SessionTracker<ukm::builders::XR_WebXR_Session>(std::move(entry)),
      receiver_(this) {}

WebXRSessionTracker::~WebXRSessionTracker() = default;

void WebXRSessionTracker::RecordRequestedFeatures(
    const device::mojom::XRSessionOptions& session_options,
    const std::set<device::mojom::XRSessionFeature>& enabled_features) {
  using device::mojom::XRSessionFeature;
  using device::mojom::XRSessionFeatureRequestStatus;

  // Set all features as 'not requested', to begin
  SetFeatureRequest(XRSessionFeature::REF_SPACE_VIEWER,
                    XRSessionFeatureRequestStatus::kNotRequested);
  SetFeatureRequest(XRSessionFeature::REF_SPACE_LOCAL,
                    XRSessionFeatureRequestStatus::kNotRequested);
  SetFeatureRequest(XRSessionFeature::REF_SPACE_LOCAL_FLOOR,
                    XRSessionFeatureRequestStatus::kNotRequested);
  SetFeatureRequest(XRSessionFeature::REF_SPACE_BOUNDED_FLOOR,
                    XRSessionFeatureRequestStatus::kNotRequested);
  SetFeatureRequest(XRSessionFeature::REF_SPACE_UNBOUNDED,
                    XRSessionFeatureRequestStatus::kNotRequested);
  // Not currently recording metrics for
  // XRSessionFeature::DOM_OVERLAY_FOR_HANDHELD_AR

  // Record required feature requests
  for (auto feature : session_options.required_features) {
    DCHECK(enabled_features.find(feature) != enabled_features.end());
    SetFeatureRequest(feature, XRSessionFeatureRequestStatus::kRequired);
  }

  // Record optional feature requests
  for (auto feature : session_options.optional_features) {
    bool enabled = enabled_features.find(feature) != enabled_features.end();
    SetFeatureRequest(
        feature, enabled ? XRSessionFeatureRequestStatus::kOptionalAccepted
                         : XRSessionFeatureRequestStatus::kOptionalRejected);
  }
}

void WebXRSessionTracker::ReportFeatureUsed(
    device::mojom::XRSessionFeature feature) {
  using device::mojom::XRSessionFeature;

  switch (feature) {
    case XRSessionFeature::REF_SPACE_VIEWER:
      ukm_entry_->SetFeatureUse_Viewer(true);
      break;
    case XRSessionFeature::REF_SPACE_LOCAL:
      ukm_entry_->SetFeatureUse_Local(true);
      break;
    case XRSessionFeature::REF_SPACE_LOCAL_FLOOR:
      ukm_entry_->SetFeatureUse_LocalFloor(true);
      break;
    case XRSessionFeature::REF_SPACE_BOUNDED_FLOOR:
      ukm_entry_->SetFeatureUse_BoundedFloor(true);
      break;
    case XRSessionFeature::REF_SPACE_UNBOUNDED:
      ukm_entry_->SetFeatureUse_Unbounded(true);
      break;
    case XRSessionFeature::DOM_OVERLAY_FOR_HANDHELD_AR:
      // Not recording metrics for this feature currently
      break;
  }
}

mojo::PendingRemote<device::mojom::XRSessionMetricsRecorder>
WebXRSessionTracker::BindMetricsRecorderPipe() {
  return receiver_.BindNewPipeAndPassRemote();
}

void WebXRSessionTracker::SetFeatureRequest(
    device::mojom::XRSessionFeature feature,
    device::mojom::XRSessionFeatureRequestStatus status) {
  using device::mojom::XRSessionFeature;

  switch (feature) {
    case XRSessionFeature::REF_SPACE_VIEWER:
      ukm_entry_->SetFeatureRequest_Viewer(static_cast<int64_t>(status));
      break;
    case XRSessionFeature::REF_SPACE_LOCAL:
      ukm_entry_->SetFeatureRequest_Local(static_cast<int64_t>(status));
      break;
    case XRSessionFeature::REF_SPACE_LOCAL_FLOOR:
      ukm_entry_->SetFeatureRequest_Local(static_cast<int64_t>(status));
      break;
    case XRSessionFeature::REF_SPACE_BOUNDED_FLOOR:
      ukm_entry_->SetFeatureRequest_Local(static_cast<int64_t>(status));
      break;
    case XRSessionFeature::REF_SPACE_UNBOUNDED:
      ukm_entry_->SetFeatureRequest_Local(static_cast<int64_t>(status));
      break;
    case XRSessionFeature::DOM_OVERLAY_FOR_HANDHELD_AR:
      // Not recording metrics for this feature currently.
      break;
  }
}

// SessionTimer will monitor the time between calls to StartSession and
// StopSession.  It will combine multiple segments into a single session if they
// are sufficiently close in time.  It will also only include segments if they
// are sufficiently long.
// Because the session may be extended, the accumulated time is occasionally
// sent on destruction or when a new session begins.
class SessionTimer {
 public:
  SessionTimer(char const* histogram_name,
               base::TimeDelta gap_time,
               base::TimeDelta minimum_duration) {
    histogram_name_ = histogram_name;
    maximum_session_gap_time_ = gap_time;
    minimum_duration_ = minimum_duration;
  }

  ~SessionTimer() { StopSession(false, base::Time::Now()); }

  void StartSession(base::Time start_time) {
    // If the new start time is within the minimum session gap time from the
    // last stop, continue the previous session. Otherwise, start a new session,
    // sending the event for the last session.
    if (!stop_time_.is_null() &&
        start_time - stop_time_ <= maximum_session_gap_time_) {
      // Mark the previous segment as non-continuable, sending data and clearing
      // state.
      StopSession(false, stop_time_);
    }

    start_time_ = start_time;
  }

  void StopSession(bool continuable, base::Time stop_time) {
    // first accumulate time from this segment of the session
    base::TimeDelta segment_duration =
        (start_time_.is_null() ? base::TimeDelta() : stop_time - start_time_);
    if (!segment_duration.is_zero() && segment_duration > minimum_duration_) {
      accumulated_time_ = accumulated_time_ + segment_duration;
    }

    if (continuable) {
      // if we are continuable, accumulate the current segment to the session,
      // and set stop_time_ so we may continue later
      accumulated_time_ = stop_time - start_time_ + accumulated_time_;
      stop_time_ = stop_time;
      start_time_ = base::Time();
    } else {
      // send the histogram now if we aren't continuable, clearing segment state
      SendAccumulatedSessionTime();

      // clear out start/stop/accumulated time
      start_time_ = base::Time();
      stop_time_ = base::Time();
      accumulated_time_ = base::TimeDelta();
    }
  }

 private:
  void SendAccumulatedSessionTime() {
    if (!accumulated_time_.is_zero()) {
      base::UmaHistogramCustomTimes(histogram_name_, accumulated_time_,
                                    base::TimeDelta(),
                                    base::TimeDelta::FromHours(5), 100);
    }
  }

  char const* histogram_name_;

  base::Time start_time_;
  base::Time stop_time_;
  base::TimeDelta accumulated_time_;

  // Config members.
  // Maximum time gap allowed between a StopSession and a StartSession before it
  // will be logged as a separate session.
  base::TimeDelta maximum_session_gap_time_;

  // Minimum time between a StartSession and StopSession required before it is
  // added to the duration.
  base::TimeDelta minimum_duration_;

  DISALLOW_COPY_AND_ASSIGN(SessionTimer);
};

// static
SessionMetricsHelper* SessionMetricsHelper::FromWebContents(
    content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!web_contents)
    return NULL;
  SessionMetricsHelperData* data = static_cast<SessionMetricsHelperData*>(
      web_contents->GetUserData(kSessionMetricsHelperDataKey));
  return data ? data->get() : NULL;
}

// static
SessionMetricsHelper* SessionMetricsHelper::CreateForWebContents(
    content::WebContents* contents,
    Mode initial_mode) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // This is not leaked as the SessionMetricsHelperData will clean it up.
  return new SessionMetricsHelper(contents, initial_mode);
}

SessionMetricsHelper::SessionMetricsHelper(content::WebContents* contents,
                                           Mode initial_mode) {
  DVLOG(2) << __func__;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(contents);

  num_videos_playing_ = contents->GetCurrentlyPlayingVideoCount();
  is_fullscreen_ = contents->IsFullscreen();
  origin_ = contents->GetLastCommittedURL();

  is_webvr_ = initial_mode == Mode::kWebXrVrPresentation;
  is_vr_enabled_ = initial_mode != Mode::kNoVr;

  session_timer_ =
      std::make_unique<SessionTimer>("VRSessionTime", kMaximumHeadsetSessionGap,
                                     kMinimumHeadsetSessionDuration);
  session_video_timer_ = std::make_unique<SessionTimer>(
      "VRSessionVideoTime", kMaximumVideoSessionGap,
      kMinimumVideoSessionDuration);

  Observe(contents);
  contents->SetUserData(kSessionMetricsHelperDataKey,
                        std::make_unique<SessionMetricsHelperData>(this));

  UpdateMode();
}

SessionMetricsHelper::~SessionMetricsHelper() {
  DVLOG(2) << __func__;
}

void SessionMetricsHelper::UpdateMode() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  Mode mode;
  if (!is_vr_enabled_) {
    mode = Mode::kNoVr;
  } else if (is_webvr_) {
    mode = Mode::kWebXrVrPresentation;
  } else {
    mode =
        is_fullscreen_ ? Mode::kVrBrowsingFullscreen : Mode::kVrBrowsingRegular;
  }

  if (mode != mode_)
    SetVrMode(mode);
}

void SessionMetricsHelper::RecordVrStartAction(VrStartAction action) {
  if (!page_session_tracker_ || mode_ == Mode::kNoVr) {
    pending_page_session_start_action_ = action;
  } else {
    LogVrStartAction(action);
  }
}

WebXRSessionTracker* SessionMetricsHelper::RecordInlineSessionStart(
    size_t session_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  DCHECK(webxr_inline_session_trackers_.find(session_id) ==
         webxr_inline_session_trackers_.end());

  auto result = webxr_inline_session_trackers_.emplace(
      session_id,
      std::make_unique<WebXRSessionTracker>(
          std::make_unique<ukm::builders::XR_WebXR_Session>(
              ukm::GetSourceIdForWebContentsDocument(web_contents()))));
  auto* tracker = result.first->second.get();

  // TODO(https://crbug.com/968546): StartAction is currently not present in
  // XR.WebXR.Session event. Remove this & change the below code with
  // replacement metrics once they are designed:
  // result.first->second->ukm_entry()->SetStartAction(
  //    PresentationStartAction::kOther);
  // TODO(crbug.com/1021212): Remove IsLegacyWebVR when safe.
  tracker->ukm_entry()->SetIsLegacyWebVR(false).SetMode(
      static_cast<int64_t>(device::SessionMode::kInline));

  return tracker;
}

void SessionMetricsHelper::RecordInlineSessionStop(size_t session_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto it = webxr_inline_session_trackers_.find(session_id);

  if (it == webxr_inline_session_trackers_.end())
    return;

  it->second->SetSessionEnd(base::Time::Now());
  it->second->ukm_entry()->SetDuration(
      it->second->GetRoundedDurationInSeconds());
  it->second->RecordEntry();

  webxr_inline_session_trackers_.erase(it);
}

WebXRSessionTracker* SessionMetricsHelper::GetImmersiveSessionTracker() {
  return webxr_immersive_session_tracker_.get();
}

WebXRSessionTracker* SessionMetricsHelper::RecordImmersiveSessionStart() {
  DCHECK(!webxr_immersive_session_tracker_);
  webxr_immersive_session_tracker_ = std::make_unique<WebXRSessionTracker>(
      std::make_unique<ukm::builders::XR_WebXR_Session>(
          ukm::GetSourceIdForWebContentsDocument(web_contents())));
  return webxr_immersive_session_tracker_.get();
}

void SessionMetricsHelper::RecordImmersiveSessionStop() {
  DCHECK(webxr_immersive_session_tracker_);
  webxr_immersive_session_tracker_->SetSessionEnd(base::Time::Now());
  webxr_immersive_session_tracker_->ukm_entry()->SetDuration(
      webxr_immersive_session_tracker_->GetRoundedDurationInSeconds());
  webxr_immersive_session_tracker_->RecordEntry();
  webxr_immersive_session_tracker_ = nullptr;
}

void SessionMetricsHelper::RecordPresentationStartAction(
    PresentationStartAction action,
    const device::mojom::XRRuntimeSessionOptions& options) {
  auto xr_session_mode = ConvertRuntimeOptionsToSessionMode(options);

  // TODO(https://crbug.com/965729): Ensure we correctly handle AR cases
  // throughout session metrics helper.
  if (!GetImmersiveSessionTracker() || mode_ != Mode::kWebXrVrPresentation) {
    pending_immersive_session_start_info_ =
        PendingImmersiveSessionStartInfo{action, xr_session_mode};
  } else {
    LogPresentationStartAction(action, xr_session_mode);
  }
}

void SessionMetricsHelper::ReportRequestPresent(
    const device::mojom::XRRuntimeSessionOptions& options) {
  DCHECK(options.immersive);

  // TODO(https://crbug.com/965729): Ensure we correctly handle AR cases
  // throughout session metrics helper.
  switch (mode_) {
    case Mode::kNoVr:
      // If we're not in VR, log this as an entry into VR from 2D.
      RecordVrStartAction(VrStartAction::kPresentationRequest);
      RecordPresentationStartAction(
          PresentationStartAction::kRequestFrom2dBrowsing, options);
      return;

    case Mode::kVr:
    case Mode::kVrBrowsing:
    case Mode::kVrBrowsingRegular:
    case Mode::kVrBrowsingFullscreen:
    case Mode::kWebXrVrPresentation:
      RecordPresentationStartAction(
          PresentationStartAction::kRequestFromVrBrowsing, options);
      return;
  }

  NOTREACHED();
}

void SessionMetricsHelper::LogVrStartAction(VrStartAction action) {
  DCHECK(page_session_tracker_);

  UMA_HISTOGRAM_ENUMERATION("XR.VRSession.StartAction", action);
  if (action == VrStartAction::kHeadsetActivation ||
      action == VrStartAction::kPresentationRequest) {
    page_session_tracker_->ukm_entry()->SetEnteredVROnPageReason(
        static_cast<int64_t>(action));
  }
}

void SessionMetricsHelper::LogPresentationStartAction(
    PresentationStartAction action,
    device::SessionMode xr_session_mode) {
  DCHECK(GetImmersiveSessionTracker());

  UMA_HISTOGRAM_ENUMERATION("XR.WebXR.PresentationSession", action);

  // TODO(https://crbug.com/968546): StartAction is currently not present in
  // XR.WebXR.Session event. Remove this & change the below code with
  // replacement metrics once they are designed:
  // webxr_immersive_session_tracker_->ukm_entry()->SetStartAction(action);
  // TODO(crbug.com/1021212): Remove IsLegacyWebVR when safe.
  GetImmersiveSessionTracker()->ukm_entry()->SetIsLegacyWebVR(false).SetMode(
      static_cast<int64_t>(xr_session_mode));
}

void SessionMetricsHelper::SetWebVREnabled(bool is_webvr_presenting) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  is_webvr_ = is_webvr_presenting;
  UpdateMode();
}

void SessionMetricsHelper::SetVRActive(bool is_vr_enabled) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  is_vr_enabled_ = is_vr_enabled;
  UpdateMode();
}

void SessionMetricsHelper::RecordVoiceSearchStarted() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  num_voice_search_started_++;
}

void SessionMetricsHelper::RecordUrlRequested(GURL url,
                                              NavigationMethod method) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  last_requested_url_ = url;
  last_url_request_method_ = method;
}

void SessionMetricsHelper::SetVrMode(Mode new_mode) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_NE(new_mode, mode_);
  DCHECK(new_mode == Mode::kVrBrowsingRegular ||
         new_mode == Mode::kVrBrowsingFullscreen ||
         new_mode == Mode::kWebXrVrPresentation || new_mode == Mode::kNoVr);

  base::Time switch_time = base::Time::Now();

  if (mode_ == Mode::kWebXrVrPresentation) {
    OnExitPresentation();
  }

  // If we are switching out of VR, stop all the session timers and record.
  if (new_mode == Mode::kNoVr) {
    OnExitAllVr();
  }

  // Stop the previous mode timers, if any.
  if (mode_ != Mode::kNoVr) {
    if (num_videos_playing_ > 0)
      mode_video_timer_->StopSession(false, switch_time);

    mode_timer_->StopSession(false, switch_time);
  }

  // Set the new trackers and timers.
  if (new_mode == Mode::kVrBrowsingRegular) {
    OnEnterRegularBrowsing();
  }

  if (new_mode == Mode::kVrBrowsingFullscreen) {
    OnEnterFullscreenBrowsing();
  }

  if (new_mode == Mode::kWebXrVrPresentation) {
    OnEnterPresentation();
  }

  // If we are switching from no VR to any kind of VR, start the new VR session
  // timers.
  if (mode_ == Mode::kNoVr) {
    OnEnterAnyVr();
  }

  // Start the new mode timers.
  if (new_mode != Mode::kNoVr) {
    mode_timer_->StartSession(switch_time);
    if (num_videos_playing_ > 0) {
      mode_video_timer_->StartSession(switch_time);
    }
  }

  mode_ = new_mode;
}

void SessionMetricsHelper::OnEnterAnyVr() {
  base::Time switch_time = base::Time::Now();
  session_timer_->StartSession(switch_time);
  num_session_video_playback_ = 0;
  num_session_navigation_ = 0;
  num_voice_search_started_ = 0;

  if (num_videos_playing_ > 0) {
    session_video_timer_->StartSession(switch_time);
    num_session_video_playback_ = num_videos_playing_;
  }

  page_session_tracker_ =
      std::make_unique<SessionTracker<ukm::builders::XR_PageSession>>(
          std::make_unique<ukm::builders::XR_PageSession>(
              ukm::GetSourceIdForWebContentsDocument(web_contents())));
  if (pending_page_session_start_action_) {
    LogVrStartAction(*pending_page_session_start_action_);
    pending_page_session_start_action_ = base::nullopt;
  }
}

void SessionMetricsHelper::OnExitAllVr() {
  base::Time switch_time = base::Time::Now();
  if (num_videos_playing_ > 0)
    session_video_timer_->StopSession(false, switch_time);

  session_timer_->StopSession(false, switch_time);

  UMA_HISTOGRAM_COUNTS_100("VRSessionVideoCount", num_session_video_playback_);
  UMA_HISTOGRAM_COUNTS_100("VRSessionNavigationCount", num_session_navigation_);
  UMA_HISTOGRAM_COUNTS_100("VR.Session.VoiceSearch.StartedCount",
                           num_voice_search_started_);

  // Do not assume page_session_tracker_ is set because it's possible that it
  // is null if DidStartNavigation has already submitted and cleared
  // page_session_tracker and DidFinishNavigation has not yet created the new
  // one.
  if (page_session_tracker_) {
    page_session_tracker_->SetSessionEnd(switch_time);
    page_session_tracker_->ukm_entry()->SetDuration(
        page_session_tracker_->GetRoundedDurationInSeconds());
    page_session_tracker_->RecordEntry();
    page_session_tracker_ = nullptr;
  }
}

void SessionMetricsHelper::OnEnterRegularBrowsing() {
  mode_timer_ = std::make_unique<SessionTimer>("VRSessionTime.Browser",
                                               kMaximumHeadsetSessionGap,
                                               kMinimumHeadsetSessionDuration);
  mode_video_timer_ = std::make_unique<SessionTimer>(
      "VRSessionVideoTime.Browser", kMaximumHeadsetSessionGap,
      kMinimumHeadsetSessionDuration);
}

void SessionMetricsHelper::OnEnterPresentation() {
  mode_timer_ = std::make_unique<SessionTimer>("VRSessionTime.WebVR",
                                               kMaximumHeadsetSessionGap,
                                               kMinimumHeadsetSessionDuration);

  mode_video_timer_ = std::make_unique<SessionTimer>(
      "VRSessionVideoTime.WebVR", kMaximumHeadsetSessionGap,
      kMinimumHeadsetSessionDuration);

  // If we are switching to WebVR presentation, start the new presentation
  // session tracker, if it hasn't been started already.
  if (!GetImmersiveSessionTracker()) {
    RecordImmersiveSessionStart();
  }

  // TODO(https://crbug.com/967764): Can pending_immersive_session_start_info_
  // be not set? What is the ordering of calls to RecordPresentationStartAction?
  auto start_info = pending_immersive_session_start_info_.value_or(
      PendingImmersiveSessionStartInfo{PresentationStartAction::kOther,
                                       device::SessionMode::kUnknown});

  LogPresentationStartAction(start_info.action, start_info.mode);
}

void SessionMetricsHelper::OnExitPresentation() {
  // If we are switching off WebVR presentation, then the presentation session
  // is done. As with the page session, do not assume
  // webxr_immersive_session_tracker_ is valid.
  if (GetImmersiveSessionTracker()) {
    RecordImmersiveSessionStop();
  }
}

void SessionMetricsHelper::OnEnterFullscreenBrowsing() {
  mode_timer_ = std::make_unique<SessionTimer>("VRSessionTime.Fullscreen",
                                               kMaximumHeadsetSessionGap,
                                               kMinimumHeadsetSessionDuration);

  mode_video_timer_ = std::make_unique<SessionTimer>(
      "VRSessionVideoTime.Fullscreen", kMaximumHeadsetSessionGap,
      kMinimumHeadsetSessionDuration);

  if (page_session_tracker_)
    page_session_tracker_->ukm_entry()->SetEnteredFullscreen(1);
}

void SessionMetricsHelper::MediaStartedPlaying(
    const MediaPlayerInfo& media_info,
    const content::MediaPlayerId&) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!media_info.has_video)
    return;

  if (num_videos_playing_ == 0) {
    // started playing video - start sessions
    base::Time start_time = base::Time::Now();

    if (mode_ != Mode::kNoVr) {
      session_video_timer_->StartSession(start_time);
      mode_video_timer_->StartSession(start_time);
    }
  }

  num_videos_playing_++;
  num_session_video_playback_++;
}

void SessionMetricsHelper::MediaStoppedPlaying(
    const MediaPlayerInfo& media_info,
    const content::MediaPlayerId&,
    WebContentsObserver::MediaStoppedReason reason) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!media_info.has_video)
    return;

  num_videos_playing_--;

  if (num_videos_playing_ == 0) {
    // stopped playing video - update existing video sessions
    base::Time stop_time = base::Time::Now();

    if (mode_ != Mode::kNoVr) {
      session_video_timer_->StopSession(true, stop_time);
      mode_video_timer_->StopSession(true, stop_time);
    }
  }
}

void SessionMetricsHelper::DidStartNavigation(
    content::NavigationHandle* handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (handle && handle->IsInMainFrame() && !handle->IsSameDocument()) {
    if (page_session_tracker_) {
      page_session_tracker_->SetSessionEnd(base::Time::Now());
      page_session_tracker_->ukm_entry()->SetDuration(
          page_session_tracker_->GetRoundedDurationInSeconds());
      page_session_tracker_->RecordEntry();
      page_session_tracker_ = nullptr;
    }

    if (GetImmersiveSessionTracker()) {
      RecordImmersiveSessionStop();
    }

    for (auto& inline_session_tracker : webxr_inline_session_trackers_) {
      inline_session_tracker.second->SetSessionEnd(base::Time::Now());
      inline_session_tracker.second->ukm_entry()->SetDuration(
          inline_session_tracker.second->GetRoundedDurationInSeconds());
      inline_session_tracker.second->RecordEntry();
    }

    webxr_inline_session_trackers_.clear();
  }
}

void SessionMetricsHelper::DidFinishNavigation(
    content::NavigationHandle* handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Counting the number of pages viewed is difficult - some websites load
  // new content dynamically without a navigation.  Others redirect several
  // times for a single navigation.
  // We look at the number of committed navigations in the main frame, which
  // will slightly overestimate pages viewed instead of trying to filter or
  // look at page loads, since those will underestimate on some pages, and
  // overestimate on others.
  if (handle && handle->HasCommitted() && handle->IsInMainFrame()) {
    origin_ = handle->GetURL();

    // Get the ukm::SourceId from the handle so that we don't wind up with a
    // wrong ukm::SourceId from this WebContentObserver perhaps executing after
    // another which changes the SourceId.
    ukm::SourceId source_id = ukm::ConvertToSourceId(
        handle->GetNavigationId(), ukm::SourceIdType::NAVIGATION_ID);
    page_session_tracker_ =
        std::make_unique<SessionTracker<ukm::builders::XR_PageSession>>(
            std::make_unique<ukm::builders::XR_PageSession>(source_id));
    if (pending_page_session_start_action_) {
      LogVrStartAction(*pending_page_session_start_action_);
      pending_page_session_start_action_ = base::nullopt;
    }

    // Check that the completed navigation is indeed the one that was requested
    // by either voice or omnibox entry, in case the requested navigation was
    // incomplete when another was begun. Check against the first entry for the
    // navigation, as redirects might have changed what the URL looks like.
    if (last_requested_url_ == handle->GetRedirectChain().front()) {
      switch (last_url_request_method_) {
        case kOmniboxUrlEntry:
        case kOmniboxSuggestionSelected:
          page_session_tracker_->ukm_entry()->SetWasOmniboxNavigation(1);
          break;
        case kVoiceSearch:
          page_session_tracker_->ukm_entry()->SetWasVoiceSearchNavigation(1);
          break;
      }
    }
    last_requested_url_ = GURL();

    if (mode_ == Mode::kWebXrVrPresentation) {
      // Start the immersive session tracker if it hasn't already
      if (!GetImmersiveSessionTracker()) {
        RecordImmersiveSessionStart();
      }
      if (pending_immersive_session_start_info_) {
        // TODO(https://crbug.com/968546): StartAction is currently not present
        // in XR.WebXR.Session event. Remove this & change the below code with
        // replacement metrics once they are designed:
        // webxr_immersive_session_tracker_->ukm_entry()->SetStartAction(
        //    pending_immersive_session_start_info_->action);
        // TODO(crbug.com/1021212): Remove IsLegacyWebVR when safe.
        GetImmersiveSessionTracker()
            ->ukm_entry()
            ->SetIsLegacyWebVR(false)
            .SetMode(static_cast<int64_t>(
                pending_immersive_session_start_info_->mode));
        pending_immersive_session_start_info_ = base::nullopt;
      }
    }

    num_session_navigation_++;
  }
}

void SessionMetricsHelper::DidToggleFullscreenModeForTab(
    bool entered_fullscreen,
    bool will_cause_resize) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  is_fullscreen_ = entered_fullscreen;
  UpdateMode();
}

}  // namespace vr
