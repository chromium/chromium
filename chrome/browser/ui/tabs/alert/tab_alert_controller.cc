// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/alert/tab_alert_controller.h"

#include <functional>
#include <optional>
#include <vector>

#include "base/check.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "chrome/browser/actor/ui/actor_ui_tab_controller.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/ui/recently_audible_helper.h"
#include "chrome/browser/ui/tabs/alert/tab_alert.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_capability_type.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/browser_ui/glic_tab_indicator_helper.h"
#include "chrome/browser/glic/public/context/glic_sharing_manager.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#endif  // BUILDFLAG(ENABLE_GLIC)

namespace tabs {

DEFINE_USER_DATA(TabAlertController);

bool CompareAlerts::operator()(TabAlert first, TabAlert second) const {
  // Alerts are ordered from highest priority to be shown to lowest priority.
  static constexpr auto tab_alert_priority =
      base::MakeFixedFlatMap<TabAlert, int>(
          {{TabAlert::kDesktopCapturing, 17},
           {TabAlert::kTabCapturing, 16},
           {TabAlert::kMediaRecording, 15},
           {TabAlert::kAudioRecording, 14},
           {TabAlert::kVideoRecording, 13},
           {TabAlert::kBluetoothConnected, 12},
           {TabAlert::kBluetoothScanActive, 11},
           {TabAlert::kUsbConnected, 10},
           {TabAlert::kHidConnected, 9},
           {TabAlert::kSerialConnected, 8},
           {TabAlert::kActorWaitingOnUser, 7},
           {TabAlert::kActorAccessing, 6},
           {TabAlert::kGlicAccessing, 5},
           {TabAlert::kGlicSharing, 4},
           // NOTE: VR must take priority over the audio alert ones
           // because most VR content has audio and its usage is implied by the
           // VR icon.
           {TabAlert::kVrPresentingInHeadset, 3},
           {TabAlert::kPipPlaying, 2},
           {TabAlert::kAudioMuting, 1},
           {TabAlert::kAudioPlaying, 0}});

  return tab_alert_priority.at(first) > tab_alert_priority.at(second);
}

TabAlertController::TabAlertController(TabInterface& tab)
    : tabs::ContentsObservingTabFeature(tab),
      scoped_unowned_user_data_(tab.GetUnownedUserDataHost(), *this) {
  media_stream_capture_indicator_observation_.Observe(
      MediaCaptureDevicesDispatcher::GetInstance()
          ->GetMediaStreamCaptureIndicator()
          .get());
  vr_tab_helper_observation_.Observe(
      vr::VrTabHelper::FromWebContents(web_contents()));
  recently_audible_subscription_ =
      RecentlyAudibleHelper::FromWebContents(tab.GetContents())
          ->RegisterRecentlyAudibleChangedCallback(base::BindRepeating(
              &TabAlertController::OnRecentlyAudibleStateChanged,
              base::Unretained(this)));

  if (auto* actor_ui_tab_controller =
          actor::ui::ActorUiTabController::From(&tab)) {
    actor_tab_indicator_callback_runner_ =
        actor_ui_tab_controller->RegisterActorTabIndicatorStateChangedCallback(
            base::BindRepeating(
                &TabAlertController::OnActorTabIndicatorStateChanged,
                base::Unretained(this)));
  }

#if BUILDFLAG(ENABLE_GLIC)
  glic::GlicTabIndicatorHelper* const glic_tab_indicator_helper =
      glic::GlicTabIndicatorHelper::From(&tab);
  if (glic_tab_indicator_helper) {
    callback_subscriptions_.emplace_back(
        glic_tab_indicator_helper->RegisterGlicSharingStateChange(
            base::BindRepeating(&TabAlertController::OnGlicSharingStateChange,
                                base::Unretained(this))));
    callback_subscriptions_.emplace_back(
        glic_tab_indicator_helper->RegisterGlicAccessingStateChange(
            base::BindRepeating(&TabAlertController::OnGlicAccessingStateChange,
                                base::Unretained(this))));
  }
#endif  // BUILDFLAG(ENABLE_GLIC)
}

TabAlertController::~TabAlertController() = default;

// static:
const TabAlertController* TabAlertController::From(const TabInterface* tab) {
  return Get(tab->GetUnownedUserDataHost());
}

// static:
TabAlertController* TabAlertController::From(TabInterface* tab) {
  return Get(tab->GetUnownedUserDataHost());
}

// static:
std::u16string TabAlertController::GetTabAlertStateText(
    const tabs::TabAlert alert_state) {
  switch (alert_state) {
    case tabs::TabAlert::kAudioPlaying:
      return l10n_util::GetStringUTF16(
          IDS_TOOLTIP_TAB_ALERT_STATE_AUDIO_PLAYING);
    case tabs::TabAlert::kAudioMuting:
      return l10n_util::GetStringUTF16(
          IDS_TOOLTIP_TAB_ALERT_STATE_AUDIO_MUTING);
    case tabs::TabAlert::kMediaRecording:
      return l10n_util::GetStringUTF16(
          IDS_TOOLTIP_TAB_ALERT_STATE_MEDIA_RECORDING);
    case tabs::TabAlert::kAudioRecording:
      return l10n_util::GetStringUTF16(
          IDS_TOOLTIP_TAB_ALERT_STATE_AUDIO_RECORDING);
    case tabs::TabAlert::kVideoRecording:
      return l10n_util::GetStringUTF16(
          IDS_TOOLTIP_TAB_ALERT_STATE_VIDEO_RECORDING);
    case tabs::TabAlert::kTabCapturing:
      return l10n_util::GetStringUTF16(
          IDS_TOOLTIP_TAB_ALERT_STATE_TAB_CAPTURING);
    case tabs::TabAlert::kBluetoothConnected:
      return l10n_util::GetStringUTF16(
          IDS_TOOLTIP_TAB_ALERT_STATE_BLUETOOTH_CONNECTED);
    case tabs::TabAlert::kBluetoothScanActive:
      return l10n_util::GetStringUTF16(
          IDS_TOOLTIP_TAB_ALERT_STATE_BLUETOOTH_SCAN_ACTIVE);
    case tabs::TabAlert::kUsbConnected:
      return l10n_util::GetStringUTF16(
          IDS_TOOLTIP_TAB_ALERT_STATE_USB_CONNECTED);
    case tabs::TabAlert::kHidConnected:
      return l10n_util::GetStringUTF16(
          IDS_TOOLTIP_TAB_ALERT_STATE_HID_CONNECTED);
    case tabs::TabAlert::kSerialConnected:
      return l10n_util::GetStringUTF16(
          IDS_TOOLTIP_TAB_ALERT_STATE_SERIAL_CONNECTED);
    case tabs::TabAlert::kPipPlaying:
      return l10n_util::GetStringUTF16(IDS_TOOLTIP_TAB_ALERT_STATE_PIP_PLAYING);
    case tabs::TabAlert::kDesktopCapturing:
      return l10n_util::GetStringUTF16(
          IDS_TOOLTIP_TAB_ALERT_STATE_DESKTOP_CAPTURING);
    case tabs::TabAlert::kVrPresentingInHeadset:
      return l10n_util::GetStringUTF16(
          IDS_TOOLTIP_TAB_ALERT_STATE_VR_PRESENTING);
    // TODO(crbug.com/461457730) Create new resources for ACTOR_ACCESSING of
    // relying on GLIC_ACCESSING resources below.
    case tabs::TabAlert::kActorAccessing:
    case tabs::TabAlert::kActorWaitingOnUser:
    case tabs::TabAlert::kGlicAccessing:
#if BUILDFLAG(ENABLE_GLIC)
      return l10n_util::GetStringUTF16(
          IDS_TOOLTIP_TAB_ALERT_STATE_GLIC_ACCESSING);
#else
      return u"";
#endif
    case tabs::TabAlert::kGlicSharing:
#if BUILDFLAG(ENABLE_GLIC)
      return l10n_util::GetStringUTF16(
          IDS_TOOLTIP_TAB_ALERT_STATE_GLIC_SHARING);
#else
      return u"";
#endif
  }
  NOTREACHED();
}

base::CallbackListSubscription
TabAlertController::AddAlertToShowChangedCallback(
    AlertToShowChangedCallback callback) {
  return alert_to_show_changed_callbacks_.Add(std::move(callback));
}

std::optional<TabAlert> TabAlertController::GetAlertToShow() const {
  if (active_alerts_.empty()) {
    return std::nullopt;
  }

  return *active_alerts_.begin();
}

std::vector<TabAlert> TabAlertController::GetAllActiveAlerts() const {
  return base::ToVector(active_alerts_);
}

bool TabAlertController::IsAlertActive(TabAlert alert) const {
  return active_alerts_.contains(alert);
}

void TabAlertController::OnDiscardContents(TabInterface* tab_interface,
                                           content::WebContents* old_contents,
                                           content::WebContents* new_contents) {
  tabs::ContentsObservingTabFeature::OnDiscardContents(
      tab_interface, old_contents, new_contents);
  vr_tab_helper_observation_.Reset();
  vr_tab_helper_observation_.Observe(
      vr::VrTabHelper::FromWebContents(new_contents));
  recently_audible_subscription_ =
      RecentlyAudibleHelper::FromWebContents(new_contents)
          ->RegisterRecentlyAudibleChangedCallback(base::BindRepeating(
              &TabAlertController::OnRecentlyAudibleStateChanged,
              base::Unretained(this)));
}

void TabAlertController::OnCapabilityTypesChanged(
    content::WebContentsCapabilityType capability_type,
    bool used) {
  static constexpr base::fixed_flat_map<content::WebContentsCapabilityType,
                                        TabAlert, 5>
      capability_type_to_alert =
          base::MakeFixedFlatMap<content::WebContentsCapabilityType, TabAlert>(
              {{content::WebContentsCapabilityType::kBluetoothConnected,
                TabAlert::kBluetoothConnected},
               {content::WebContentsCapabilityType::kBluetoothScanning,
                TabAlert::kBluetoothScanActive},
               {content::WebContentsCapabilityType::kUSB,
                TabAlert::kUsbConnected},
               {content::WebContentsCapabilityType::kHID,
                TabAlert::kHidConnected},
               {content::WebContentsCapabilityType::kSerial,
                TabAlert::kSerialConnected}});

  if (!capability_type_to_alert.contains(capability_type)) {
    return;
  }

  const TabAlert alert = capability_type_to_alert.at(capability_type);
  UpdateAlertState(alert, used);
}

void TabAlertController::MediaPictureInPictureChanged(
    bool is_picture_in_picture) {
  UpdateAlertState(TabAlert::kPipPlaying, is_picture_in_picture);
}

void TabAlertController::DidUpdateAudioMutingState(bool muted) {
  // The muted alert should only show for tabs that were recently audible. It is
  // possible for a tab to be muted but never play audio, in such cases, the
  // muted alert should not show.
  RecentlyAudibleHelper* const audible_helper =
      RecentlyAudibleHelper::FromWebContents(tab().GetContents());
  CHECK(audible_helper);
  UpdateAlertState(TabAlert::kAudioMuting,
                   audible_helper->WasRecentlyAudible() && muted);
}

void TabAlertController::OnIsCapturingVideoChanged(
    content::WebContents* contents,
    bool is_capturing_video) {
  if (contents == web_contents()) {
    UpdateAlertState(TabAlert::kVideoRecording, is_capturing_video);
  }
}

void TabAlertController::OnIsCapturingAudioChanged(
    content::WebContents* contents,
    bool is_capturing_audio) {
  if (contents == web_contents()) {
    UpdateAlertState(TabAlert::kAudioRecording, is_capturing_audio);
  }
}

void TabAlertController::OnIsBeingMirroredChanged(
    content::WebContents* contents,
    bool is_being_mirrored) {
  if (contents == web_contents()) {
    UpdateAlertState(TabAlert::kTabCapturing, is_being_mirrored);
  }
}

void TabAlertController::OnIsCapturingWindowChanged(
    content::WebContents* contents,
    bool is_capturing_window) {
  if (contents == web_contents()) {
    const bool is_desktop_capturing_active =
        is_capturing_window || MediaCaptureDevicesDispatcher::GetInstance()
                                   ->GetMediaStreamCaptureIndicator()
                                   ->IsCapturingDisplay(contents);
    UpdateAlertState(TabAlert::kDesktopCapturing, is_desktop_capturing_active);
  }
}

void TabAlertController::OnIsCapturingDisplayChanged(
    content::WebContents* contents,
    bool is_capturing_display) {
  if (contents == web_contents()) {
    const bool is_desktop_capturing_active =
        is_capturing_display || MediaCaptureDevicesDispatcher::GetInstance()
                                    ->GetMediaStreamCaptureIndicator()
                                    ->IsCapturingWindow(contents);
    UpdateAlertState(TabAlert::kDesktopCapturing, is_desktop_capturing_active);
  }
}

void TabAlertController::OnIsContentDisplayedInHeadsetChanged(bool state) {
  UpdateAlertState(TabAlert::kVrPresentingInHeadset, state);
}

#if BUILDFLAG(ENABLE_GLIC)
void TabAlertController::OnGlicSharingStateChange(bool is_sharing) {
  UpdateAlertState(TabAlert::kGlicSharing, is_sharing);
}

void TabAlertController::OnGlicAccessingStateChange(bool is_accessing) {
  UpdateAlertState(TabAlert::kGlicAccessing, is_accessing);
}
#endif  // BUILDFLAG(ENABLE_GLIC)

void TabAlertController::OnActorTabIndicatorStateChanged(
    actor::ui::TabIndicatorStatus tab_indicator_status) {
  switch (tab_indicator_status) {
    case actor::ui::TabIndicatorStatus::kNone:
      UpdateAlertState(TabAlert::kActorWaitingOnUser, false);
      UpdateAlertState(TabAlert::kActorAccessing, false);
      break;
    case actor::ui::TabIndicatorStatus::kDynamic:
      UpdateAlertState(TabAlert::kActorWaitingOnUser, false);
      UpdateAlertState(TabAlert::kActorAccessing, true);
      break;
    case actor::ui::TabIndicatorStatus::kStatic:
      UpdateAlertState(TabAlert::kActorWaitingOnUser, true);
      UpdateAlertState(TabAlert::kActorAccessing, false);
      break;
  }
}

void TabAlertController::OnRecentlyAudibleStateChanged(bool was_audible) {
  // Muted alert state also needs to update when audible state changes to ensure
  // that the muted alert becomes active if the tab is already muted but is
  // recently audible or inactive after the tab is no longer audible.
  DidUpdateAudioMutingState(tab().GetContents()->IsAudioMuted());
  UpdateAlertState(TabAlert::kAudioPlaying, was_audible);
}

void TabAlertController::UpdateAlertState(TabAlert alert, bool is_active) {
  std::optional<TabAlert> previous_alert = GetAlertToShow();

  if (alert == TabAlert::kAudioRecording ||
      alert == TabAlert::kVideoRecording) {
    UpdateMediaAlert();
  } else {
    if (is_active) {
      active_alerts_.insert(alert);
    } else {
      active_alerts_.erase(alert);
    }
  }

  std::optional<TabAlert> updated_alert = GetAlertToShow();
  if (previous_alert != updated_alert) {
    alert_to_show_changed_callbacks_.Notify(updated_alert);
  }
}

void TabAlertController::UpdateMediaAlert() {
  MediaStreamCaptureIndicator* const media_stream_capture_indicator =
      MediaCaptureDevicesDispatcher::GetInstance()
          ->GetMediaStreamCaptureIndicator()
          .get();
  content::WebContents* const web_contents = tab().GetContents();

  const bool is_capturing_audio =
      media_stream_capture_indicator->IsCapturingAudio(web_contents);
  const bool is_capturing_video =
      media_stream_capture_indicator->IsCapturingVideo(web_contents);

  active_alerts_.erase(TabAlert::kMediaRecording);
  active_alerts_.erase(TabAlert::kVideoRecording);
  active_alerts_.erase(TabAlert::kAudioRecording);

  if (is_capturing_video && is_capturing_audio) {
    active_alerts_.insert(TabAlert::kMediaRecording);
  } else if (is_capturing_video) {
    active_alerts_.insert(TabAlert::kVideoRecording);
  } else if (is_capturing_audio) {
    active_alerts_.insert(TabAlert::kAudioRecording);
  }
}
}  // namespace tabs
