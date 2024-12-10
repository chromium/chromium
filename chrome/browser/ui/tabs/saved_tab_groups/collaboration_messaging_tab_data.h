// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_COLLABORATION_MESSAGING_TAB_DATA_H_
#define CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_COLLABORATION_MESSAGING_TAB_DATA_H_

#include "base/callback_list.h"
#include "base/strings/utf_string_conversions.h"
#include "components/collaboration/public/messaging/message.h"

namespace tab_groups {

using collaboration::messaging::CollaborationEvent;
using collaboration::messaging::PersistentMessage;

class CollaborationMessagingTabData {
 public:
  using CallbackList = base::RepeatingCallbackList<void()>;

  CollaborationMessagingTabData();
  CollaborationMessagingTabData(CollaborationMessagingTabData& other) = delete;
  CollaborationMessagingTabData& operator=(
      CollaborationMessagingTabData& other) = delete;
  CollaborationMessagingTabData(CollaborationMessagingTabData&& other) = delete;
  CollaborationMessagingTabData& operator=(
      CollaborationMessagingTabData&& other) = delete;
  ~CollaborationMessagingTabData();

  void SetMessage(PersistentMessage message);
  void ClearMessage(PersistentMessage message);
  bool HasMessage() { return message_.has_value(); }

  base::CallbackListSubscription RegisterMessageChangedCallback(
      CallbackList::CallbackType cb);
  void NotifyMessageChanged();

  std::u16string given_name() {
    CHECK(HasMessage());

    auto member = message_->attribution.triggering_user;
    CHECK(member.has_value());

    return base::UTF8ToUTF16(member->given_name);
  }

  GURL avatar_url() {
    CHECK(HasMessage());

    auto member = message_->attribution.triggering_user;
    CHECK(member.has_value());

    return member->avatar_url;
  }

  CollaborationEvent collaboration_event() {
    CHECK(HasMessage());
    return message_->collaboration_event;
  }

  base::WeakPtr<CollaborationMessagingTabData> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  std::optional<PersistentMessage> message_ = std::nullopt;

  // Listeners to notify when the message for this tab changes.
  CallbackList message_changed_callback_list_;

  // Must be the last member.
  base::WeakPtrFactory<CollaborationMessagingTabData> weak_factory_{this};
};

}  // namespace tab_groups

#endif  // CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_COLLABORATION_MESSAGING_TAB_DATA_H_
