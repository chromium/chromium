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
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/ui/recently_audible_helper.h"
#include "chrome/browser/ui/tabs/alert/tab_alert.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_capability_type.h"

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/public/context/glic_sharing_manager.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#endif  // BUILDFLAG(ENABLE_GLIC)

namespace {
glic::GlicKeyedService* GetGlicKeyedService(
    BrowserWindowInterface* browser_window_interface) {
#if BUILDFLAG(ENABLE_GLIC)
  return glic::GlicKeyedService::Get(browser_window_interface->GetProfile());
#else
  return nullptr;
#endif  // BUILDFLAG(ENABLE_GLIC)
}
}  // namespace

namespace tabs {

bool CompareAlerts::operator()(TabAlert first, TabAlert second) const {
  // Alerts are ordered from highest priority to be shown to lowest priority.
  static constexpr auto tab_alert_priority =
      base::MakeFixedFlatMap<TabAlert, int>(
          {{TabAlert::DESKTOP_CAPTURING, 15},
           {TabAlert::TAB_CAPTURING, 14},
           {TabAlert::MEDIA_RECORDING, 13},
           {TabAlert::AUDIO_RECORDING, 12},
           {TabAlert::VIDEO_RECORDING, 11},
           {TabAlert::BLUETOOTH_CONNECTED, 10},
           {TabAlert::BLUETOOTH_SCAN_ACTIVE, 9},
           {TabAlert::USB_CONNECTED, 8},
           {TabAlert::HID_CONNECTED, 7},
           {TabAlert::SERIAL_CONNECTED, 6},
           {TabAlert::GLIC_ACCESSING, 5},
           {TabAlert::GLIC_SHARING, 4},
           {TabAlert::VR_PRESENTING_IN_HEADSET, 3},
           {TabAlert::PIP_PLAYING, 2},
           {TabAlert::AUDIO_MUTING, 1},
           {TabAlert::AUDIO_PLAYING, 0}});

  return tab_alert_priority.at(first) > tab_alert_priority.at(second);
}

TabAlertController::TabAlertController(TabInterface& tab)
    : TabAlertController(tab,
                         GetGlicKeyedService(tab.GetBrowserWindowInterface())) {
}

TabAlertController::TabAlertController(
    TabInterface& tab,
    glic::GlicKeyedService* glic_keyed_service)
    : tabs::ContentsObservingTabFeature(tab) {
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

#if BUILDFLAG(ENABLE_GLIC)
  if (glic_keyed_service) {
    callback_subscriptions_.push_back(
        glic_keyed_service->AddContextAccessIndicatorStatusChangedCallback(
            base::BindRepeating(
                &TabAlertController::OnGlicContextAccessIndicatorStatusChanged,
                base::Unretained(this))));
    glic::GlicSharingManager& glic_sharing_manager =
        glic_keyed_service->sharing_manager();
    callback_subscriptions_.emplace_back(
        glic_sharing_manager.AddFocusedTabChangedCallback(base::BindRepeating(
            &TabAlertController::OnGlicSharingFocusedTabChanged,
            base::Unretained(this))));
    callback_subscriptions_.emplace_back(
        glic_sharing_manager.AddTabPinningStatusChangedCallback(
            base::BindRepeating(&TabAlertController::OnGlicTabPinningChanged,
                                base::Unretained(this))));
  }
#endif  // BUILDFLAG(ENABLE_GLIC)
}

TabAlertController::~TabAlertController() = default;

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

std::vector<TabAlert> TabAlertController::GetAllActiveAlerts() {
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
                TabAlert::BLUETOOTH_CONNECTED},
               {content::WebContentsCapabilityType::kBluetoothScanning,
                TabAlert::BLUETOOTH_SCAN_ACTIVE},
               {content::WebContentsCapabilityType::kUSB,
                TabAlert::USB_CONNECTED},
               {content::WebContentsCapabilityType::kHID,
                TabAlert::HID_CONNECTED},
               {content::WebContentsCapabilityType::kSerial,
                TabAlert::SERIAL_CONNECTED}});

  if (!capability_type_to_alert.contains(capability_type)) {
    return;
  }

  const TabAlert alert = capability_type_to_alert.at(capability_type);
  UpdateAlertState(alert, used);
}

void TabAlertController::MediaPictureInPictureChanged(
    bool is_picture_in_picture) {
  UpdateAlertState(TabAlert::PIP_PLAYING, is_picture_in_picture);
}

void TabAlertController::DidUpdateAudioMutingState(bool muted) {
  UpdateAlertState(TabAlert::AUDIO_MUTING, muted);
}

void TabAlertController::OnIsCapturingVideoChanged(
    content::WebContents* contents,
    bool is_capturing_video) {
  if (contents == web_contents()) {
    UpdateAlertState(TabAlert::MEDIA_RECORDING, is_capturing_video);
  }
}

void TabAlertController::OnIsCapturingAudioChanged(
    content::WebContents* contents,
    bool is_capturing_audio) {
  if (contents == web_contents()) {
    UpdateAlertState(TabAlert::MEDIA_RECORDING, is_capturing_audio);
  }
}

void TabAlertController::OnIsBeingMirroredChanged(
    content::WebContents* contents,
    bool is_being_mirrored) {
  if (contents == web_contents()) {
    UpdateAlertState(TabAlert::TAB_CAPTURING, is_being_mirrored);
  }
}

void TabAlertController::OnIsCapturingWindowChanged(
    content::WebContents* contents,
    bool is_capturing_window) {
  if (contents == web_contents()) {
    UpdateAlertState(TabAlert::DESKTOP_CAPTURING, is_capturing_window);
  }
}

void TabAlertController::OnIsCapturingDisplayChanged(
    content::WebContents* contents,
    bool is_capturing_display) {
  if (contents == web_contents()) {
    UpdateAlertState(TabAlert::DESKTOP_CAPTURING, is_capturing_display);
  }
}

void TabAlertController::OnIsContentDisplayedInHeadsetChanged(bool state) {
  UpdateAlertState(TabAlert::VR_PRESENTING_IN_HEADSET, state);
}

#if BUILDFLAG(ENABLE_GLIC)
void TabAlertController::OnGlicContextAccessIndicatorStatusChanged(
    bool is_accessing) {
  UpdateAlertState(TabAlert::GLIC_ACCESSING,
                   GetGlicKeyedService(tab().GetBrowserWindowInterface())
                       ->IsContextAccessIndicatorShown(tab().GetContents()));
}

void TabAlertController::OnGlicSharingFocusedTabChanged(
    const glic::FocusedTabData& focused_tab_data) {
  const bool is_alert_active =
      focused_tab_data.focus() != &tab()
          ? false
          : GetGlicKeyedService(tab().GetBrowserWindowInterface())
                ->IsContextAccessIndicatorShown(tab().GetContents());
  UpdateAlertState(TabAlert::GLIC_ACCESSING, is_alert_active);
}

void TabAlertController::OnGlicTabPinningChanged(
    tabs::TabInterface* tab_interface,
    bool is_sharing) {
  if (tab_interface->GetContents() == web_contents()) {
    UpdateAlertState(TabAlert::GLIC_SHARING, is_sharing);
  }
}
#endif  // BUILDFLAG(ENABLE_GLIC)

void TabAlertController::OnRecentlyAudibleStateChanged(bool was_audible) {
  UpdateAlertState(TabAlert::AUDIO_PLAYING, was_audible);
}

void TabAlertController::UpdateAlertState(TabAlert alert, bool is_active) {
  std::optional<TabAlert> previous_alert = GetAlertToShow();
  if (is_active) {
    active_alerts_.insert(alert);
  } else {
    active_alerts_.erase(alert);
  }

  std::optional<TabAlert> updated_alert = GetAlertToShow();
  if (previous_alert != updated_alert) {
    alert_to_show_changed_callbacks_.Notify(updated_alert);
  }
}
}  // namespace tabs
