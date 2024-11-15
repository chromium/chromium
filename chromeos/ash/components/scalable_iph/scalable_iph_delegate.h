// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SCALABLE_IPH_SCALABLE_IPH_DELEGATE_H_
#define CHROMEOS_ASH_COMPONENTS_SCALABLE_IPH_SCALABLE_IPH_DELEGATE_H_

#include <ostream>

#include "base/observer_list_types.h"
#include "chromeos/ash/components/scalable_iph/iph_session.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph_constants.h"

namespace scalable_iph {

// `ScalableIphDelegate` is responsible to delegate tasks to Chrome or Ash. In
// contrast, `ScalableIph` is responsible to decide on which/when to trigger an
// IPH.
//
// This delegate is responsible for:
// - Show an IPH with a request from `ScalableIph`.
// - Observe events in Ash, e.g. Network state change, etc.
class ScalableIphDelegate {
 public:
  enum class SessionState { kUnknownInitialValue, kActive, kLocked, kOther };

  // Observer for observing events in Ash.
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnConnectionChanged(bool online) {}

    // Called when `SessionState` is changed.
    virtual void OnSessionStateChanged(SessionState session_state) {}

    // Called when the device does not enables lock screen, and every time the
    // system resumes from suspension.
    virtual void OnSuspendDoneWithoutLockScreen() {}

    // Called when the visibility of an app list has changed.
    virtual void OnAppListVisibilityChanged(bool shown) {}

    // Called when there is a change in whether there is a saved printer or not.
    // This method is called only if there is a change in a value. Initial value
    // is expected to be `false`.
    virtual void OnHasSavedPrintersChanged(bool has_saved_printers) {}

    // Called when there is a change in eligibility of phone hub onboarding.
    // This method is called only if there is a change in a value. Initial value
    // is expected to be `false`.
    virtual void OnPhoneHubOnboardingEligibleChanged(
        bool phonehub_onboarding_eligible) {}
  };

  // Have a virtual destructor as we can put `ScalableIphDelegate` in
  // unique_ptr.
  virtual ~ScalableIphDelegate() = default;

  struct Action {
    ActionType action_type = ActionType::kInvalid;

    // An event name notified to the feature engagement framework on the
    // execution of this action. Typically this event name will be set to
    // `event_used` of an event config.
    std::string iph_event_name;
    bool operator==(const Action& action) const = default;
  };

  struct Button {
    std::string text;
    Action action;

    bool operator==(const Button& button) const = default;
  };

  enum class BubbleIcon {
    kNoIcon,
    kChromeIcon,
    kPlayStoreIcon,
    kGoogleDocsIcon,
    kGooglePhotosIcon,
    kPrintJobsIcon,
    kYouTubeIcon,
    kLastIcon = kYouTubeIcon,
  };

  struct BubbleParams {
    BubbleParams();
    BubbleParams(const BubbleParams&);
    BubbleParams& operator=(const BubbleParams&);
    ~BubbleParams();

    std::string bubble_id;
    std::string title;
    std::string text;
    BubbleIcon icon = BubbleIcon::kNoIcon;
    Button button;
    std::string anchor_view_app_id;

    bool operator==(const BubbleParams& params) const = default;
  };

  // TODO(b/284158831): Define types of notifications, such as wallpaper,
  // printer, etc.
  enum class NotificationImageType {
    kNoImage = 0,
    kWallpaper,
    kMinecraft,
  };

  enum class NotificationIcon {
    kDefault,
    kRedeem,
  };

  enum class NotificationSummaryText {
    kNone,
    kWelcomeTips,
  };

  struct NotificationParams {
    NotificationParams();
    NotificationParams(const NotificationParams&);
    NotificationParams& operator=(const NotificationParams&);
    ~NotificationParams();

    NotificationImageType image_type = NotificationImageType::kNoImage;
    std::string notification_id;
    std::string title;
    std::string text;
    std::string source = kCustomNotificationSourceTextValueDefault;
    NotificationIcon icon = NotificationIcon::kDefault;
    NotificationSummaryText summary_text =
        NotificationSummaryText::kWelcomeTips;
    Button button;

    bool operator==(const NotificationParams& params) const = default;
  };

  // Deliver a bubble UI IPH to a user with specified behavior via
  // `BubbleParams`. A delegate must show an IPH if this method gets called.
  // Note that `IphSession` has a reference to `feature_engagement::Tracker`. Do
  // NOT interact it after a `Tracker` service `Shutdown`. `ScalableIphDelegate`
  // is owned by `ScalableIph` keyed service. `ScalableIph` keyed service
  // depends on `Tracker` keyed service and `ScalableIph` keyed service
  // destrcuts this `ScalableIphDelegate` in `ScalableIph::Shutdown`. Do NOT
  // interact with `IphSession` once the destructor gets called. Returns true if
  // it has requested the UI framework to show a bubble. Note that it's not
  // guaranteed that the UI framework has actually shown a bubble.
  virtual bool ShowBubble(const BubbleParams& params,
                          std::unique_ptr<IphSession> iph_session) = 0;

  // Same with `ShowBubble` method. But this method delivers a notification UI
  // IPH to a user with specified behavior via `NotificationParams`. Returns
  // true if it has requested the UI framework to show a notification. Note that
  // it's not guaranteed that the UI framework has actually shown a
  // notification.
  virtual bool ShowNotification(const NotificationParams& params,
                                std::unique_ptr<IphSession> iph_session) = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Returns true if a device is online.
  virtual bool IsOnline() = 0;

  // Returns client age in days. The day count starts from 0. Day 0 means the
  // first 24 hours. Note that this can return a negative number if a profile
  // creation time is in a future time for some reason, e.g. Clock has changed.
  virtual int ClientAgeInDays() = 0;

  // Performs `action_type` in Ash or Chrome. This method is for `ScalableIph`
  // keyed service to delegate actions. Other code (e.g. Ui code) MUST use
  // `PerformAction` in `IphSession` or `ScalableIph`.
  virtual void PerformActionForScalableIph(ActionType action_type) = 0;
};

std::ostream& operator<<(std::ostream& out,
                         ScalableIphDelegate::SessionState session_state);
std::ostream& operator<<(std::ostream& out, ScalableIphDelegate::Action action);
std::ostream& operator<<(std::ostream& out, ScalableIphDelegate::Button button);
std::ostream& operator<<(std::ostream& out,
                         ScalableIphDelegate::BubbleIcon bubble_icon);
std::ostream& operator<<(std::ostream& out,
                         ScalableIphDelegate::BubbleParams bubble_params);
std::ostream& operator<<(
    std::ostream& out,
    ScalableIphDelegate::NotificationImageType notification_image_type);
std::ostream& operator<<(
    std::ostream& out,
    ScalableIphDelegate::NotificationIcon notification_icon);
std::ostream& operator<<(
    std::ostream& out,
    ScalableIphDelegate::NotificationSummaryText summary_text);
std::ostream& operator<<(std::ostream& out,
                         ScalableIphDelegate::NotificationParams params);

}  // namespace scalable_iph

#endif  // CHROMEOS_ASH_COMPONENTS_SCALABLE_IPH_SCALABLE_IPH_DELEGATE_H_
