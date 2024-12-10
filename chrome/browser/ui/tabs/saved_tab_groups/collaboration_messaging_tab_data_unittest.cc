// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/collaboration_messaging_tab_data.h"

#include "base/test/mock_callback.h"
#include "components/collaboration/public/messaging/message.h"
#include "components/data_sharing/public/group_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tab_groups {

using collaboration::messaging::CollaborationEvent;
using collaboration::messaging::MessageAttribution;
using collaboration::messaging::PersistentMessage;
using collaboration::messaging::PersistentNotificationType;
using data_sharing::GroupMember;

class CollaborationMessagingTabDataTest : public testing::Test {
 protected:
  void SetUp() override {
    tab_data_ = std::make_unique<CollaborationMessagingTabData>();
  }

  CollaborationMessagingTabData& tab_data() { return *tab_data_; }

  PersistentMessage CreateMessage(std::string given_name,
                                  std::string avatar_url,
                                  CollaborationEvent event) {
    GroupMember member;
    member.given_name = given_name;
    member.avatar_url = GURL(avatar_url);

    MessageAttribution attribution;
    attribution.triggering_user = member;

    PersistentMessage message;
    message.type = PersistentNotificationType::CHIP;
    message.attribution = attribution;
    message.collaboration_event = event;

    return message;
  }

 private:
  std::unique_ptr<CollaborationMessagingTabData> tab_data_;
};

TEST_F(CollaborationMessagingTabDataTest, CanSetAndClearData) {
  EXPECT_FALSE(tab_data().HasMessage());

  std::string given_name = "User";
  std::string avatar_url = "URL";
  auto message =
      CreateMessage(given_name, avatar_url, CollaborationEvent::TAB_ADDED);
  tab_data().SetMessage(message);
  EXPECT_TRUE(tab_data().HasMessage());
  EXPECT_EQ(tab_data().given_name(), base::UTF8ToUTF16(given_name));
  EXPECT_EQ(tab_data().avatar_url(), GURL(avatar_url));
  EXPECT_EQ(tab_data().collaboration_event(), CollaborationEvent::TAB_ADDED);

  // Overwrite with a new message.
  std::string given_name2 = "User2";
  std::string avatar_url2 = "URL2";
  auto message2 =
      CreateMessage(given_name2, avatar_url2, CollaborationEvent::TAB_UPDATED);
  tab_data().SetMessage(message2);
  EXPECT_TRUE(tab_data().HasMessage());
  EXPECT_EQ(tab_data().given_name(), base::UTF8ToUTF16(given_name2));
  EXPECT_EQ(tab_data().avatar_url(), GURL(avatar_url2));
  EXPECT_EQ(tab_data().collaboration_event(), CollaborationEvent::TAB_UPDATED);

  tab_data().ClearMessage(message2);
  EXPECT_FALSE(tab_data().HasMessage());
}

TEST_F(CollaborationMessagingTabDataTest, NotifiesListeners) {
  EXPECT_FALSE(tab_data().HasMessage());

  auto message = CreateMessage("User", "URL", CollaborationEvent::TAB_ADDED);

  base::MockCallback<CollaborationMessagingTabData::CallbackList::CallbackType>
      cb;
  auto subscription = tab_data().RegisterMessageChangedCallback(cb.Get());

  // Callback is called when message is set.
  EXPECT_CALL(cb, Run);
  tab_data().SetMessage(message);
  EXPECT_TRUE(tab_data().HasMessage());

  // Callback is called again when message is cleared.
  EXPECT_CALL(cb, Run);
  tab_data().ClearMessage(message);
  EXPECT_FALSE(tab_data().HasMessage());
}

}  // namespace tab_groups
