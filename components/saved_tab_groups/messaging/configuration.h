// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_MESSAGING_CONFIGURATION_H_
#define COMPONENTS_SAVED_TAB_GROUPS_MESSAGING_CONFIGURATION_H_

#include <cstdint>

#include "components/saved_tab_groups/messaging/message.h"

namespace tab_groups::messaging {
inline constexpr uint64_t DEFAULT_INSTANT_MESSAGE_SECONDS_TTL =
    3600 * 24 * 7;  // One week.
inline constexpr uint64_t DEFAULT_PERSISTENT_MESSAGE_SECONDS_TTL =
    3600 * 24 * 7;  // One week.

// Generic requirement enum describing a requirement to be Yes, No, or if Any
// value is OK.
enum class Requirement {
  kAny,
  kYes,
  kNo,
};

// Requirements about the current open/close and focus state of the browser.
struct BrowserStateRequirement {
 public:
  // Whether the Chrome application is currently in the foreground.
  Requirement app_foregrounded = Requirement::kYes;

  // Whether the tab group needs to be open (different from focused).
  Requirement tab_group_open = Requirement::kYes;

  // Whether the window the tab group is in is currently focused.
  Requirement window_focused = Requirement::kAny;

  // Whether the tab group needs to be focused.
  Requirement tab_group_focused = Requirement::kAny;

  // Whether the tab needs to be focused.
  Requirement tab_focused = Requirement::kAny;
};

// Requirements about the involved user.
enum class UserRequirement {
  kAny,            // Any user is OK.
  kOnlyOtherUser,  // Requires to be about someone else than the current user.
  kOnlySelf,       // Requires to be about the current user.
};

// Describes what to do in the case of requirements not being met, e.g.
// waiting until they are met or dropping them.
enum class DelayPolicy {
  kKeepForever,
  kKeepUntilTTL,
  kDrop,
};

// Base class for all message configurations.
struct MessageConfigBase {
 public:
  MessageConfigBase(UserAction user_action,
                    BrowserStateRequirement display_requirement,
                    UserRequirement user_requirement,
                    DelayPolicy delay_policy,
                    uint64_t delay_policy_ttl);
  virtual ~MessageConfigBase();

  // Which action was taken.
  UserAction user_action;

  // What are the requirements for the browser state for displaying the message.
  BrowserStateRequirement display_requirement;

  // What are the requirements for who initiated a particular action.
  UserRequirement initiator_user_requirement;

  // What to do if requirements are not met.
  DelayPolicy delay_policy;

  // TTL to use if delay_policy is `kWaitUntilTTL`.
  uint64_t delay_policy_ttl_seconds;
};

// Contains configuration about an instant notification.
struct InstantMessageConfig : public MessageConfigBase {
 public:
  InstantMessageConfig(
      UserAction user_action,
      UserRequirement user_requirement,
      BrowserStateRequirement display_requirement,
      InstantNotificationLevel notification_level,
      InstantNotificationType notification_type,
      DelayPolicy delay_policy,
      uint64_t delay_policy_ttl = DEFAULT_INSTANT_MESSAGE_SECONDS_TTL);
  ~InstantMessageConfig() override;

  // What level of notification should be used for the message.
  InstantNotificationLevel notification_level;

  // What type of notification should be used for the message.
  InstantNotificationType notification_type;
};

// Contains configuration about a persistent notification.
struct PersistentMessageConfig : public MessageConfigBase {
 public:
  PersistentMessageConfig(
      UserAction user_action,
      UserRequirement user_requirement,
      BrowserStateRequirement display_requirement,
      BrowserStateRequirement hide_requirement,
      PersistentNotificationType notification_type,
      DelayPolicy delay_policy,
      uint64_t delay_policy_ttl = DEFAULT_PERSISTENT_MESSAGE_SECONDS_TTL);
  ~PersistentMessageConfig() override;

  // What are the requirements for the browser state for hiding the message.
  BrowserStateRequirement hide_requirement;

  // What type of notification should be used for the message.
  PersistentNotificationType notification_type;
};

}  // namespace tab_groups::messaging

#endif  // COMPONENTS_SAVED_TAB_GROUPS_MESSAGING_CONFIGURATION_H_
