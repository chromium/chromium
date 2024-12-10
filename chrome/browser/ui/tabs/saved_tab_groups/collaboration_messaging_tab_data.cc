// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/collaboration_messaging_tab_data.h"

#include "components/collaboration/public/messaging/message.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/l10n/l10n_util.h"

using collaboration::messaging::PersistentMessage;

namespace tab_groups {

CollaborationMessagingTabData::CollaborationMessagingTabData() = default;
CollaborationMessagingTabData::~CollaborationMessagingTabData() = default;

void CollaborationMessagingTabData::SetMessage(PersistentMessage message) {
  using collaboration::messaging::CollaborationEvent;
  using collaboration::messaging::PersistentNotificationType;

  // Only Chip messages are allowed.
  CHECK(message.type == PersistentNotificationType::CHIP);

  // Chip messages are always TAB_ADDED or TAB_UPDATED.
  CHECK(message.collaboration_event == CollaborationEvent::TAB_ADDED ||
        message.collaboration_event == CollaborationEvent::TAB_UPDATED);

  message_ = message;
  NotifyMessageChanged();
}

void CollaborationMessagingTabData::ClearMessage(PersistentMessage message) {
  message_ = std::nullopt;
  NotifyMessageChanged();
}

base::CallbackListSubscription
CollaborationMessagingTabData::RegisterMessageChangedCallback(
    CallbackList::CallbackType cb) {
  return message_changed_callback_list_.Add(std::move(cb));
}

void CollaborationMessagingTabData::NotifyMessageChanged() {
  message_changed_callback_list_.Notify();
}

}  // namespace tab_groups
