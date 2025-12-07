// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/collaboration_messaging_tab_data.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/data_sharing/data_sharing_service_factory.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/test/base/testing_profile.h"
#include "components/collaboration/public/messaging/message.h"
#include "components/data_sharing/public/data_sharing_service.h"
#include "components/data_sharing/public/group_data.h"
#include "components/data_sharing/test_support/mock_data_sharing_service.h"
#include "components/signin/public/base/avatar_icon_util.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

namespace tab_groups {

using collaboration::messaging::CollaborationEvent;
using collaboration::messaging::MessageAttribution;
using collaboration::messaging::PersistentMessage;
using collaboration::messaging::PersistentNotificationType;
using data_sharing::GroupMember;
using testing::_;

namespace {
const int kAvatarSize = signin::kAccountInfoImageSize;

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

}  // namespace

class CollaborationMessagingTabDataTest : public testing::Test {
 protected:
  void SetUp() override {
    TestingProfile::Builder builder;

    auto sharing_service =
        std::make_unique<data_sharing::MockDataSharingService>();
    sharing_service_ = sharing_service.get();

    builder.AddTestingFactory(
        data_sharing::DataSharingServiceFactory::GetInstance(),
        base::BindOnce(
            [](std::unique_ptr<data_sharing::MockDataSharingService>
                   sharing_service,
               content::BrowserContext* context)
                -> std::unique_ptr<KeyedService> { return sharing_service; },
            std::move(sharing_service)));

    testing_profile_ = builder.Build();

    EXPECT_CALL(mock_tab_interface_, GetUnownedUserDataHost())
        .WillRepeatedly(testing::ReturnRef(unowned_user_data_host_));

    EXPECT_CALL(mock_tab_interface_, GetBrowserWindowInterface())
        .Times(1)
        .WillRepeatedly(testing::Return(&mock_browser_window_interface_));

    EXPECT_CALL(mock_browser_window_interface_, GetProfile())
        .Times(1)
        .WillRepeatedly(testing::Return(profile()));

    tab_data_ =
        std::make_unique<CollaborationMessagingTabData>(&mock_tab_interface_);
  }

  TestingProfile* profile() { return testing_profile_.get(); }
  CollaborationMessagingTabData& tab_data() { return *tab_data_; }
  data_sharing::MockDataSharingService* sharing_service() {
    return sharing_service_;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  ui::UnownedUserDataHost unowned_user_data_host_;
  tabs::MockTabInterface mock_tab_interface_;
  MockBrowserWindowInterface mock_browser_window_interface_;

 private:
  std::unique_ptr<TestingProfile> testing_profile_;
  std::unique_ptr<CollaborationMessagingTabData> tab_data_;
  raw_ptr<data_sharing::MockDataSharingService> sharing_service_;
};

TEST_F(CollaborationMessagingTabDataTest, CanSetAndClearData) {
  EXPECT_FALSE(tab_data().HasMessage());

  std::string given_name = "User";
  std::string avatar_url = "https://google.com/chrome/1";
  auto message =
      CreateMessage(given_name, avatar_url, CollaborationEvent::TAB_ADDED);

  // Run loop to trigger network request callback.
  {
    base::RunLoop run_loop;
    auto quit_closure = run_loop.QuitClosure();

    EXPECT_CALL(*sharing_service(), GetAvatarImageForURL)
        .WillOnce([&](GURL url, int size, auto load_callback, auto fetcher) {
          EXPECT_EQ(url, GURL(message.attribution.triggering_user->avatar_url));
          EXPECT_EQ(size, kAvatarSize);

          // Trigger callback with a mock avatar.
          std::move(load_callback).Run(favicon::GetDefaultFavicon());

          quit_closure.Run();
        });

    tab_data().SetMessage(message);
    run_loop.Run();
  }

  // Data will be set.
  EXPECT_TRUE(tab_data().HasMessage());
  EXPECT_EQ(tab_data().given_name(), base::UTF8ToUTF16(given_name));
  EXPECT_EQ(tab_data().collaboration_event(), CollaborationEvent::TAB_ADDED);
  EXPECT_FALSE(tab_data().get_avatar_for_testing()->IsEmpty());

  // Overwrite with a new message.
  std::string given_name2 = "User2";
  std::string avatar_url2 = "https://google.com/chrome/2";
  auto message2 =
      CreateMessage(given_name2, avatar_url2, CollaborationEvent::TAB_UPDATED);

  // Run loop to trigger network request callback.
  {
    base::RunLoop run_loop;
    auto quit_closure = run_loop.QuitClosure();

    EXPECT_CALL(*sharing_service(), GetAvatarImageForURL)
        .WillOnce([&](GURL url, int size, auto load_callback, auto fetcher) {
          EXPECT_EQ(url,
                    GURL(message2.attribution.triggering_user->avatar_url));
          EXPECT_EQ(size, kAvatarSize);

          // Trigger callback with empty image.
          std::move(load_callback).Run(gfx::Image());

          quit_closure.Run();
        });

    tab_data().SetMessage(message2);
    run_loop.Run();
  }

  EXPECT_TRUE(tab_data().HasMessage());
  EXPECT_EQ(tab_data().given_name(), base::UTF8ToUTF16(given_name2));
  EXPECT_EQ(tab_data().collaboration_event(), CollaborationEvent::TAB_UPDATED);
  EXPECT_TRUE(tab_data().get_avatar_for_testing()->IsEmpty());

  tab_data().ClearMessage(message2);
  EXPECT_FALSE(tab_data().HasMessage());
}

TEST_F(CollaborationMessagingTabDataTest, IgnoresRequestsWhenMessageIsCleared) {
  EXPECT_FALSE(tab_data().HasMessage());

  std::string given_name = "User";
  std::string avatar_url = "https://google.com/chrome/1";
  auto message =
      CreateMessage(given_name, avatar_url, CollaborationEvent::TAB_ADDED);

  // Service will be triggered to request the image.
  EXPECT_CALL(*sharing_service(),
              GetAvatarImageForURL(GURL(avatar_url), kAvatarSize, _, _))
      .Times(1);

  // Set the message. This will trigger the avatar request, but will not
  // set data since the fetcher is mocked.
  tab_data().SetMessage(message);

  // Clear the message. This will cause the following Commit to be rejected.
  tab_data().ClearMessage(message);

  // Mock resolution of the avatar image request by directly
  // triggering the callback.
  tab_data().CommitMessage(message, gfx::Image());

  // Data will not be set.
  EXPECT_FALSE(tab_data().HasMessage());
}

TEST_F(CollaborationMessagingTabDataTest, IgnoresRequestsWhenMessageIsChanged) {
  EXPECT_FALSE(tab_data().HasMessage());

  std::string given_name = "User";
  std::string avatar_url = "https://google.com/chrome/1";
  auto message =
      CreateMessage(given_name, avatar_url, CollaborationEvent::TAB_ADDED);

  // Service will be triggered to request the image.
  EXPECT_CALL(*sharing_service(),
              GetAvatarImageForURL(GURL(avatar_url), kAvatarSize, _, _))
      .Times(1);

  // Set the message. This will trigger the avatar request, but will not
  // set data since the fetcher is mocked.
  tab_data().SetMessage(message);

  std::string given_name2 = "User2";
  std::string avatar_url2 = "https://google.com/chrome/2";
  auto message2 =
      CreateMessage(given_name2, avatar_url2, CollaborationEvent::TAB_UPDATED);

  // Service will be triggered to request the image.
  EXPECT_CALL(*sharing_service(),
              GetAvatarImageForURL(GURL(avatar_url2), kAvatarSize, _, _))
      .Times(1);

  // Set a new message. This will cause the following Commit to be rejected.
  tab_data().SetMessage(message2);

  // Attempt to commit the first message. This will be rejected.
  // Mock resolution of the avatar image request by directly
  // triggering the callback.
  tab_data().CommitMessage(message, gfx::Image());

  // Data will not be set.
  EXPECT_FALSE(tab_data().HasMessage());
}

TEST_F(CollaborationMessagingTabDataTest, IgnoresMessageWithoutUser) {
  EXPECT_FALSE(tab_data().HasMessage());

  std::string given_name = "User";
  std::string avatar_url = "https://google.com/chrome/1";
  auto message =
      CreateMessage(given_name, avatar_url, CollaborationEvent::TAB_ADDED);

  // Remove triggering_user. Not all messages will be fully formed, so tab
  // data should be able to ignore messages without enough data to display.
  message.attribution.triggering_user = std::nullopt;

  // Service will be triggered to request the image.
  EXPECT_CALL(*sharing_service(),
              GetAvatarImageForURL(GURL(avatar_url), kAvatarSize, _, _))
      .Times(0);

  // Set the message. This will trigger the avatar request, but will not
  // set data since the fetcher is mocked.
  tab_data().SetMessage(message);

  // Mock resolution of the avatar image request by directly
  // triggering the callback.
  tab_data().CommitMessage(message, gfx::Image());

  // Data will not be set.
  EXPECT_FALSE(tab_data().HasMessage());
}

// Verifies that From() function returns proper result
TEST_F(CollaborationMessagingTabDataTest, VerifyGetUnownedUserDataHost) {
  CollaborationMessagingTabData* const tab_data =
      CollaborationMessagingTabData::From(&mock_tab_interface_);

  EXPECT_FALSE(tab_data->HasMessage());
}
}  // namespace tab_groups
