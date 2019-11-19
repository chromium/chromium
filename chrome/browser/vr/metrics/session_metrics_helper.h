// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_METRICS_SESSION_METRICS_HELPER_H_
#define CHROME_BROWSER_VR_METRICS_SESSION_METRICS_HELPER_H_

#include <memory>
#include <set>

#include "base/time/time.h"
#include "chrome/browser/vr/mode.h"
#include "chrome/browser/vr/ui_browser_interface.h"
#include "chrome/browser/vr/vr_base_export.h"
#include "content/public/browser/web_contents_observer.h"
#include "device/vr/public/cpp/session_mode.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/gurl.h"

namespace vr {

// This enum describes various ways a Chrome VR session started.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Ensure that this stays in sync with VRSessionStartAction in enums.xml
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.vr
enum class VrStartAction : int {
  // The user activated a headset. For example, inserted phone in Daydream, or
  // put on an Occulus or Vive.
  kHeadsetActivation = 1,
  // The user triggered a presentation request on a page, probably by clicking
  // an enter VR button.
  kPresentationRequest = 2,
  // OBSOLETE: The user launched a deep linked app, probably from Daydream Home.
  // kDeepLinkedApp = 3,
  // Chrome VR was started by an intent from another app. Most likely the user
  // clicked the icon in Daydream home.
  kIntentLaunch = 4,
  kMaxValue = kIntentLaunch,
};

// The source of a request to enter XR Presentation.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Ensure that this stays in sync with VRPresentationStartAction in enums.xml.
enum PresentationStartAction {
  // A catch all for methods of Presentation entry that are not otherwise
  // logged.
  kOther = 0,
  // The user triggered a presentation request on a page in 2D, probably by
  // clicking an enter VR button.
  kRequestFrom2dBrowsing = 1,
  // The user triggered a presentation request on a page in VR browsing,
  // probably by clicking an enter VR button.
  kRequestFromVrBrowsing = 2,
  // The user activated a headset on a page that listens for headset activations
  // and requests presentation.
  kHeadsetActivation = 3,
  // OBSOLETE: The user launched a deep linked app, probably from Daydream Home.
  // kDeepLinkedApp = 4,
  kMaxValue = 4,
};

class SessionTimer;

// SessionTracker tracks UKM data for sessions and sends the data upon request.
template <class T>
class SessionTracker {
 public:
  explicit SessionTracker(std::unique_ptr<T> entry)
      : ukm_entry_(std::move(entry)),
        start_time_(base::Time::Now()),
        stop_time_(base::Time::Now()) {}
  virtual ~SessionTracker() {}
  T* ukm_entry() { return ukm_entry_.get(); }
  void SetSessionEnd(base::Time stop_time) { stop_time_ = stop_time; }

  int GetRoundedDurationInSeconds() {
    if (start_time_ > stop_time_) {
      // Return negative one to indicate an invalid value was recorded.
      return -1;
    }

    base::TimeDelta duration = stop_time_ - start_time_;

    if (duration.InHours() > 1) {
      return duration.InHours() * 3600;
    } else if (duration.InMinutes() > 10) {
      return (duration.InMinutes() / 10) * 10 * 60;
    } else if (duration.InSeconds() > 60) {
      return duration.InMinutes() * 60;
    } else {
      return duration.InSeconds();
    }
  }

  void RecordEntry() {
    ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
    DCHECK(ukm_recorder);

    ukm_entry_->Record(ukm_recorder);
  }

 protected:
  std::unique_ptr<T> ukm_entry_;

  base::Time start_time_;
  base::Time stop_time_;

  DISALLOW_COPY_AND_ASSIGN(SessionTracker);
};

class VR_BASE_EXPORT WebXRSessionTracker
    : public SessionTracker<ukm::builders::XR_WebXR_Session>,
      device::mojom::XRSessionMetricsRecorder {
 public:
  explicit WebXRSessionTracker(
      std::unique_ptr<ukm::builders::XR_WebXR_Session> entry);
  ~WebXRSessionTracker() override;

  // Records which features for the session have been requested as required or
  // optional, which were accepted/rejeceted, and which weren't requested at
  // all. This assumes that the session as a whole was accepted.
  void RecordRequestedFeatures(
      const device::mojom::XRSessionOptions& session_options,
      const std::set<device::mojom::XRSessionFeature>& enabled_features);

  // |XRSessionMetricsRecorder| implementation
  void ReportFeatureUsed(device::mojom::XRSessionFeature feature) override;

  // Binds this tracker's |XRSessionMetricsRecorder| receiver to a new pipe, and
  // returns the |PendingRemote|.
  mojo::PendingRemote<device::mojom::XRSessionMetricsRecorder>
  BindMetricsRecorderPipe();

 private:
  void SetFeatureRequest(device::mojom::XRSessionFeature feature,
                         device::mojom::XRSessionFeatureRequestStatus status);

  mojo::Receiver<device::mojom::XRSessionMetricsRecorder> receiver_;
};

// This class is not thread-safe and must only be used from the main thread.
// This class tracks metrics for various kinds of sessions, including VR
// browsing sessions, WebXR presentation sessions, and others. It mainly tracks
// metrics that require state monitoring, such as durations, but also tracks
// data we want attached to that, such as number of videos watched and how the
// session was started.
class VR_BASE_EXPORT SessionMetricsHelper
    : public content::WebContentsObserver {
 public:
  // Returns the SessionMetricsHelper singleton if it has been created for the
  // WebContents.
  static SessionMetricsHelper* FromWebContents(content::WebContents* contents);
  static SessionMetricsHelper* CreateForWebContents(
      content::WebContents* contents,
      Mode initial_mode);

  ~SessionMetricsHelper() override;

  // Despite the name, which may suggest WebVR 1.1, both of these are *also*
  // used for and crucial to, WebXr metrics.
  void SetWebVREnabled(bool is_webvr_presenting);
  void SetVRActive(bool is_vr_enabled);
  void RecordVoiceSearchStarted();
  void RecordUrlRequested(GURL url, NavigationMethod method);

  // TODO(https://crbug.com/967764): Add documentation to the public functions.
  // TODO(https://crbug.com/965744): Rename below methods.
  // TODO(https://crbug.com/965729): Ensure that AR is handled correctly.
  void RecordVrStartAction(VrStartAction action);
  void RecordPresentationStartAction(
      PresentationStartAction action,
      const device::mojom::XRRuntimeSessionOptions& options);
  void ReportRequestPresent(
      const device::mojom::XRRuntimeSessionOptions& options);

  // Records that inline session was started.
  WebXRSessionTracker* RecordInlineSessionStart(size_t session_id);
  // Records that inline session was stopped. Will record an UKM entry.
  void RecordInlineSessionStop(size_t session_id);

  WebXRSessionTracker* GetImmersiveSessionTracker();

  // Records that an immersive session was started. Two immersive sessions
  // may not exist simultaneously.
  WebXRSessionTracker* RecordImmersiveSessionStart();

  // Records that an immersive session was stopped. Will record a UKM entry.
  void RecordImmersiveSessionStop();

 private:
  SessionMetricsHelper(content::WebContents* contents, Mode initial_mode);

  struct PendingImmersiveSessionStartInfo {
    PresentationStartAction action = PresentationStartAction::kOther;
    device::SessionMode mode = device::SessionMode::kUnknown;
  };

  // WebContentsObserver
  void MediaStartedPlaying(const MediaPlayerInfo& media_info,
                           const content::MediaPlayerId&) override;
  void MediaStoppedPlaying(
      const MediaPlayerInfo& media_info,
      const content::MediaPlayerId&,
      WebContentsObserver::MediaStoppedReason reason) override;
  void DidStartNavigation(content::NavigationHandle* handle) override;
  void DidFinishNavigation(content::NavigationHandle* handle) override;
  void DidToggleFullscreenModeForTab(bool entered_fullscreen,
                                     bool will_cause_resize) override;

  void SetVrMode(Mode mode);
  void UpdateMode();

  void LogVrStartAction(VrStartAction action);
  void LogPresentationStartAction(PresentationStartAction action,
                                  device::SessionMode xr_session_mode);

  void OnEnterAnyVr();
  void OnExitAllVr();
  void OnEnterRegularBrowsing();
  void OnEnterPresentation();
  void OnExitPresentation();
  void OnEnterFullscreenBrowsing();

  std::unique_ptr<SessionTimer> mode_video_timer_;
  std::unique_ptr<SessionTimer> session_video_timer_;
  std::unique_ptr<SessionTimer> mode_timer_;
  std::unique_ptr<SessionTimer> session_timer_;

  std::unique_ptr<SessionTracker<ukm::builders::XR_PageSession>>
      page_session_tracker_;
  std::unique_ptr<WebXRSessionTracker> webxr_immersive_session_tracker_;

  // Map associating active inline session Ids to their trackers. The contents
  // of the map are managed by |RecordInlineSessionStart| and
  // |RecordInlineSessionStop|.
  std::unordered_map<size_t, std::unique_ptr<WebXRSessionTracker>>
      webxr_inline_session_trackers_;

  Mode mode_ = Mode::kNoVr;

  // State that gets translated into the VR mode.
  // TODO(https://crbug.com/967764): Add description for below fields and rename
  // if it turns out their purpose does not match the names.
  bool is_fullscreen_ = false;
  bool is_webvr_ = false;
  bool is_vr_enabled_ = false;

  GURL last_requested_url_;
  NavigationMethod last_url_request_method_;

  base::Optional<VrStartAction> pending_page_session_start_action_;
  base::Optional<PendingImmersiveSessionStartInfo>
      pending_immersive_session_start_info_;

  int num_videos_playing_ = 0;
  int num_session_navigation_ = 0;
  int num_session_video_playback_ = 0;
  int num_voice_search_started_ = 0;

  GURL origin_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_METRICS_SESSION_METRICS_HELPER_H_
