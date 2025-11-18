// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_ALERT_TAB_ALERT_CONTROLLER_H_
#define CHROME_BROWSER_UI_TABS_ALERT_TAB_ALERT_CONTROLLER_H_

#include <optional>
#include <vector>

#include "base/callback_list.h"
#include "base/containers/flat_set.h"
#include "base/scoped_observation.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/ui/tabs/alert/tab_alert.h"
#include "chrome/browser/ui/tabs/contents_observing_tab_feature.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace content {
enum class WebContentsCapabilityType;
class WebContents;
}  // namespace content

namespace actor::ui {
enum class TabIndicatorStatus;
}  // namespace actor::ui

namespace tabs {
class TabInterface;

// Comparator used to determine which tab alert has a higher priority to be
// shown.
struct CompareAlerts {
  bool operator()(TabAlert first, TabAlert second) const;
};

// Observes the corresponding web contents for the tab to keep track of all
// active alerts. Callers can subscribe and be notified when the tab alert that
// should be shown changes.
class TabAlertController : public tabs::ContentsObservingTabFeature,
                           public MediaStreamCaptureIndicator::Observer,
                           public vr::VrTabHelper::Observer {
 public:
  explicit TabAlertController(TabInterface& tab);
  TabAlertController(const TabAlertController&) = delete;
  TabAlertController& operator=(const TabAlertController&) = delete;
  ~TabAlertController() override;

  DECLARE_USER_DATA(TabAlertController);

  static const TabAlertController* From(const TabInterface* tab);
  static TabAlertController* From(TabInterface* tab);

  // Returns a localized string describing the `alert_state`.
  static std::u16string GetTabAlertStateText(const tabs::TabAlert alert_state);

  using AlertToShowChangedCallback =
      base::RepeatingCallback<void(std::optional<TabAlert>)>;
  base::CallbackListSubscription AddAlertToShowChangedCallback(
      AlertToShowChangedCallback callback);

  std::optional<TabAlert> GetAlertToShow() const;
  // Gets all active tab alerts that is sorted from highest priority
  // to lowest priority to be shown.
  std::vector<TabAlert> GetAllActiveAlerts() const;

  // Returns true if `alert` is currently active for this tab and false
  // otherwise.
  bool IsAlertActive(TabAlert alert) const;

  // WebContentsObserver:
  void OnDiscardContents(TabInterface* tab_interface,
                         content::WebContents* old_contents,
                         content::WebContents* new_contents) override;
  void OnCapabilityTypesChanged(
      content::WebContentsCapabilityType capability_type,
      bool used) override;
  void MediaPictureInPictureChanged(bool is_picture_in_picture) override;
  void DidUpdateAudioMutingState(bool muted) override;

  // MediaStreamCaptureIndicator::Observer:
  void OnIsCapturingVideoChanged(content::WebContents* contents,
                                 bool is_capturing_video) override;
  void OnIsCapturingAudioChanged(content::WebContents* contents,
                                 bool is_capturing_audio) override;
  void OnIsBeingMirroredChanged(content::WebContents* contents,
                                bool is_being_mirrored) override;
  void OnIsCapturingWindowChanged(content::WebContents* contents,
                                  bool is_capturing_window) override;
  void OnIsCapturingDisplayChanged(content::WebContents* contents,
                                   bool is_capturing_display) override;

  // VrTabHelper::Observer:
  void OnIsContentDisplayedInHeadsetChanged(bool state) override;

 private:
#if BUILDFLAG(ENABLE_GLIC)
  void OnGlicSharingStateChange(bool is_sharing);
  void OnGlicAccessingStateChange(bool is_accessing);
#endif  // BUILDFLAG(ENABLE_GLIC)

  void OnActorTabIndicatorStateChanged(
      actor::ui::TabIndicatorStatus tab_indicator_state);
  void OnRecentlyAudibleStateChanged(bool was_audible);

  // Adds `alert` to the set of already active alerts for this tab if it isn't
  // currently active. Otherwise, removes `alert` from the set and is considered
  // inactive.
  void UpdateAlertState(TabAlert alert, bool is_active);
  // Updates the set of active alerts with the currently active media alerts for
  // this tab.
  void UpdateMediaAlert();

  using AlertToShowChangedCallbackList =
      base::RepeatingCallbackList<void(std::optional<TabAlert>)>;
  AlertToShowChangedCallbackList alert_to_show_changed_callbacks_;

  // Maintains a sorted collection of all active tab alerts from highest
  // priority to lowest priority to be shown.
  base::flat_set<TabAlert, CompareAlerts> active_alerts_;

  // Observes the MediaStreamCaptureIndicator so the alert controller will be
  // notified when a media stream capture has changed.
  base::ScopedObservation<MediaStreamCaptureIndicator,
                          MediaStreamCaptureIndicator::Observer>
      media_stream_capture_indicator_observation_{this};

  // Observes the VrTabHelper so that the controller will be notified when a tab
  // is displaying content to a headset.
  base::ScopedObservation<vr::VrTabHelper, vr::VrTabHelper::Observer>
      vr_tab_helper_observation_{this};

  // Subscriptions to be notified when an alert status has changed.
  base::CallbackListSubscription recently_audible_subscription_;
  std::vector<base::CallbackListSubscription> callback_subscriptions_;
  base::ScopedClosureRunner actor_tab_indicator_callback_runner_;

  ui::ScopedUnownedUserData<TabAlertController> scoped_unowned_user_data_;
};
}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_ALERT_TAB_ALERT_CONTROLLER_H_
