// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_COLLABORATION_MESSAGING_TAB_DATA_H_
#define CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_COLLABORATION_MESSAGING_TAB_DATA_H_

#include "base/strings/utf_string_conversions.h"
#include "components/collaboration/public/messaging/message.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/l10n/l10n_util.h"

namespace tab_groups {

using collaboration::messaging::CollaborationEvent;
using collaboration::messaging::PersistentMessage;

class CollaborationMessagingTabData {
 public:
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

 private:
  std::optional<PersistentMessage> message_ = std::nullopt;
};

}  // namespace tab_groups

#endif  // CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_COLLABORATION_MESSAGING_TAB_DATA_H_
