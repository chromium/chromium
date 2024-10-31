// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/messaging/configuration.h"

#include "components/collaboration/public/messaging/message.h"

namespace collaboration::messaging {

MessageConfigBase::MessageConfigBase(CollaborationEvent collab_event,
                                     BrowserStateRequirement display_req,
                                     UserRequirement user_req,
                                     DelayPolicy delay_pol,
                                     uint64_t delay_pol_ttl)
    : collaboration_event(collab_event),
      display_requirement(display_req),
      initiator_user_requirement(user_req),
      delay_policy(delay_pol),
      delay_policy_ttl_seconds(delay_pol_ttl) {}

MessageConfigBase::~MessageConfigBase() = default;

InstantMessageConfig::InstantMessageConfig(
    CollaborationEvent collab_event,
    UserRequirement user_requirement,
    BrowserStateRequirement display_requirement,
    InstantNotificationLevel level,
    InstantNotificationType type,
    DelayPolicy delay_policy,
    uint64_t delay_policy_ttl)
    : MessageConfigBase(collab_event,
                        display_requirement,
                        user_requirement,
                        delay_policy,
                        delay_policy_ttl),
      notification_level(level),
      notification_type(type) {}

InstantMessageConfig::~InstantMessageConfig() = default;

PersistentMessageConfig::PersistentMessageConfig(
    CollaborationEvent collab_event,
    UserRequirement user_requirement,
    BrowserStateRequirement display_requirement,
    BrowserStateRequirement hide_req,
    PersistentNotificationType type,
    DelayPolicy delay_policy,
    uint64_t delay_policy_ttl)
    : MessageConfigBase(collab_event,
                        display_requirement,
                        user_requirement,
                        delay_policy,
                        delay_policy_ttl),
      hide_requirement(hide_req),
      notification_type(type) {}

PersistentMessageConfig::~PersistentMessageConfig() = default;

}  // namespace collaboration::messaging
