// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/messaging/configuration.h"

#include "components/saved_tab_groups/messaging/message.h"

namespace tab_groups::messaging {

MessageConfigBase::MessageConfigBase(UserAction action,
                                     BrowserStateRequirement display_req,
                                     UserRequirement user_req,
                                     DelayPolicy delay_pol,
                                     uint64_t delay_pol_ttl)
    : user_action(action),
      display_requirement(display_req),
      initiator_user_requirement(user_req),
      delay_policy(delay_pol),
      delay_policy_ttl_seconds(delay_pol_ttl) {}

MessageConfigBase::~MessageConfigBase() = default;

InstantMessageConfig::InstantMessageConfig(
    UserAction user_action,
    UserRequirement user_requirement,
    BrowserStateRequirement display_requirement,
    InstantNotificationLevel level,
    InstantNotificationType type,
    DelayPolicy delay_policy,
    uint64_t delay_policy_ttl)
    : MessageConfigBase(user_action,
                        display_requirement,
                        user_requirement,
                        delay_policy,
                        delay_policy_ttl),
      notification_level(level),
      notification_type(type) {}

InstantMessageConfig::~InstantMessageConfig() = default;

PersistentMessageConfig::PersistentMessageConfig(
    UserAction user_action,
    UserRequirement user_requirement,
    BrowserStateRequirement display_requirement,
    BrowserStateRequirement hide_req,
    PersistentNotificationType type,
    DelayPolicy delay_policy,
    uint64_t delay_policy_ttl)
    : MessageConfigBase(user_action,
                        display_requirement,
                        user_requirement,
                        delay_policy,
                        delay_policy_ttl),
      hide_requirement(hide_req),
      notification_type(type) {}

PersistentMessageConfig::~PersistentMessageConfig() = default;

}  // namespace tab_groups::messaging
