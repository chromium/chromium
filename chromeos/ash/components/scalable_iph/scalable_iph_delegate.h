// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SCALABLE_IPH_SCALABLE_IPH_DELEGATE_H_
#define CHROMEOS_ASH_COMPONENTS_SCALABLE_IPH_SCALABLE_IPH_DELEGATE_H_

#include "chromeos/ash/components/scalable_iph/iph_session.h"

namespace scalable_iph {

// `ScalableIphDelegate` is responsible to delivering IPH. `ScalableIph` class
// is responsible to triggering an IPH. After the class has decided to trigger
// an IPH, this delegate is responsible to deliver it.
class ScalableIphDelegate {
 public:
  // Have a virtul destructor as we can put `ScalableIphDelegate` in unique_ptr.
  virtual ~ScalableIphDelegate() = default;

  // TODO(b/284158779): Details of the interface TBD.
  enum class ActionType {
    // `kNoActionInvalid` is used as an initial value. This should not be used
    // in prod.
    kNoActionInvalid,
  };

  struct Action {
    ActionType action_type = ActionType::kNoActionInvalid;

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
  };

  struct BubbleParams {
    std::string text;
    BubbleIcon icon = BubbleIcon::kNoIcon;
    Button button;

    bool operator==(const BubbleParams& params) const = default;
  };

  struct NotificationParams {
    std::string title;
    std::string text;
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
  // interact with `IphSession` once the destructor gets called.
  virtual void ShowBubble(const BubbleParams& params,
                          std::unique_ptr<IphSession> iph_session) = 0;

  // Same with `ShowBubble` method. But this method delivers a notification UI
  // IPH to a user with specified behavior via `NotificationParams`.
  virtual void ShowNotification(const NotificationParams& params,
                                std::unique_ptr<IphSession> iph_session) = 0;
};

}  // namespace scalable_iph

#endif  // CHROMEOS_ASH_COMPONENTS_SCALABLE_IPH_SCALABLE_IPH_DELEGATE_H_
