// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/instant_message_queue_processor.h"

#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/data_sharing/data_sharing_service_factory.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/toasts/toast_features.h"
#include "chrome/browser/ui/toasts/toast_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/test/base/testing_profile.h"
#include "components/collaboration/public/messaging/message.h"
#include "components/data_sharing/public/data_sharing_service.h"
#include "components/data_sharing/public/group_data.h"
#include "components/data_sharing/test_support/mock_data_sharing_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tab_groups {

using collaboration::messaging::CollaborationEvent;
using collaboration::messaging::InstantMessage;
using collaboration::messaging::InstantNotificationLevel;
using collaboration::messaging::InstantNotificationType;
using collaboration::messaging::MessageAttribution;
using collaboration::messaging::PersistentNotificationType;
using collaboration::messaging::TabGroupMessageMetadata;
using collaboration::messaging::TabMessageMetadata;
using data_sharing::GroupMember;
using testing::_;

using SuccessCallback = collaboration::messaging::MessagingBackendService::
    InstantMessageDelegate::SuccessCallback;

namespace {

InstantMessage CreateMessage(CollaborationEvent event) {
  GroupMember member;
  member.given_name = "User";

  TabMessageMetadata tab_metadata;
  tab_metadata.last_known_url = "chrome://settings";
  tab_metadata.last_known_title = "Chrome settings";

  TabGroupMessageMetadata tab_group_metadata;
  tab_group_metadata.local_tab_group_id = TabGroupId::GenerateNew();
  tab_group_metadata.last_known_title = "Vacation";

  MessageAttribution attribution;
  if (event == CollaborationEvent::TAB_REMOVED) {
    attribution.triggering_user = member;
  } else {
    attribution.affected_user = member;
  }
  attribution.tab_metadata = tab_metadata;
  attribution.tab_group_metadata = tab_group_metadata;

  InstantMessage message;
  message.attributions.emplace_back(attribution);
  message.collaboration_event = event;
  message.level = InstantNotificationLevel::BROWSER;
  message.type = event == CollaborationEvent::TAB_REMOVED
                     ? InstantNotificationType::CONFLICT_TAB_REMOVED
                     : InstantNotificationType::UNDEFINED;
  message.localized_message = u"Message content";

  return message;
}

class TestInstantMessageQueueProcessor : public InstantMessageQueueProcessor {
 public:
  explicit TestInstantMessageQueueProcessor(Profile* profile)
      : InstantMessageQueueProcessor(profile) {}
  ~TestInstantMessageQueueProcessor() override = default;

  bool MaybeShowToastInBrowser(Browser* browser,
                               std::optional<ToastParams> params) override {
    return toast_will_be_shown_;
  }

  void SetToastWillBeShown(bool enabled) { toast_will_be_shown_ = enabled; }

 private:
  bool toast_will_be_shown_ = false;
};

}  // namespace

class InstantMessageQueueProcessorTest : public testing::Test {
 protected:
  void SetUp() override {
    layout_provider_ = ChromeLayoutProvider::CreateLayoutProvider();

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

    processor_ = std::make_unique<TestInstantMessageQueueProcessor>(profile());
  }

  TestingProfile* profile() { return testing_profile_.get(); }
  data_sharing::MockDataSharingService* sharing_service() {
    return sharing_service_;
  }

  TestInstantMessageQueueProcessor* processor() { return processor_.get(); }

  void FastForwardByToastDuration() {
    task_environment_.FastForwardBy(processor()->GetMessageInterval());
  }

  int GetAvatarSize() { return toasts::ToastView::GetIconSize(); }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<views::LayoutProvider> layout_provider_;
  std::unique_ptr<TestingProfile> testing_profile_;
  raw_ptr<data_sharing::MockDataSharingService> sharing_service_;
  std::unique_ptr<TestInstantMessageQueueProcessor> processor_;
};

TEST_F(InstantMessageQueueProcessorTest, IgnoresUnsupportedEvents) {
  EXPECT_FALSE(processor()->IsMessageShowing());
  EXPECT_EQ(0, processor()->GetQueueSize());

  auto message = CreateMessage(CollaborationEvent::TAB_ADDED);
  base::MockCallback<SuccessCallback> callback;

  // Message will fail because it has the wrong event type.
  EXPECT_CALL(callback, Run(false));
  processor()->Enqueue(message, callback.Get());

  EXPECT_FALSE(processor()->IsMessageShowing());
  EXPECT_EQ(0, processor()->GetQueueSize());
}

TEST_F(InstantMessageQueueProcessorTest,
       FailsMessageUnableToCreateToastParams) {
  EXPECT_FALSE(processor()->IsMessageShowing());
  EXPECT_EQ(0, processor()->GetQueueSize());

  auto message = CreateMessage(CollaborationEvent::TAB_REMOVED);

  // Remove user so that toast cannot be created from message.
  message.attributions[0].triggering_user = std::nullopt;

  base::MockCallback<SuccessCallback> callback;

  // Message will fail because toast params could not be created.
  EXPECT_CALL(callback, Run(false));
  processor()->Enqueue(message, callback.Get());

  EXPECT_FALSE(processor()->IsMessageShowing());
  EXPECT_EQ(0, processor()->GetQueueSize());
}

TEST_F(InstantMessageQueueProcessorTest, ShowsSupportedEvents) {
  // Mock that toast to be shown.
  processor()->SetToastWillBeShown(true);

  EXPECT_FALSE(processor()->IsMessageShowing());
  EXPECT_EQ(0, processor()->GetQueueSize());

  auto message = CreateMessage(CollaborationEvent::TAB_REMOVED);
  base::MockCallback<SuccessCallback> callback;

  // Message will succeed.
  EXPECT_CALL(callback, Run(true));
  processor()->Enqueue(message, callback.Get());

  EXPECT_TRUE(processor()->IsMessageShowing());
  EXPECT_EQ(1, processor()->GetQueueSize());

  // Fast forward until toast has timed out.
  FastForwardByToastDuration();

  EXPECT_FALSE(processor()->IsMessageShowing());
  EXPECT_EQ(0, processor()->GetQueueSize());
}

TEST_F(InstantMessageQueueProcessorTest, QueuesMessages) {
  // Mock that toast to be shown.
  processor()->SetToastWillBeShown(true);

  EXPECT_FALSE(processor()->IsMessageShowing());
  EXPECT_EQ(0, processor()->GetQueueSize());

  // Create messages with avatars.
  const int avatar_size = GetAvatarSize();
  auto message1 = CreateMessage(CollaborationEvent::COLLABORATION_MEMBER_ADDED);
  message1.attributions[0].affected_user->avatar_url =
      GURL("https://www.google.com/image1.png");
  auto message2 = CreateMessage(CollaborationEvent::TAB_REMOVED);
  message2.attributions[0].triggering_user->avatar_url =
      GURL("https://www.google.com/image2.png");

  base::MockCallback<SuccessCallback> callback1;
  base::MockCallback<SuccessCallback> callback2;

  // Message will succeed.
  EXPECT_CALL(callback1, Run(true));

  // Mock avatar request for message1 from enqueuing the message.
  {
    base::RunLoop run_loop;
    auto quit_closure = run_loop.QuitClosure();

    EXPECT_CALL(*sharing_service(), GetAvatarImageForURL)
        .WillOnce([&](GURL url, int size, auto load_callback, auto fetcher) {
          EXPECT_EQ(
              url,
              GURL(message1.attributions.front().affected_user->avatar_url));
          EXPECT_EQ(size, avatar_size);

          // Trigger callback with a mock avatar.
          std::move(load_callback).Run(favicon::GetDefaultFavicon());

          quit_closure.Run();
        });

    processor()->Enqueue(message1, callback1.Get());
    run_loop.Run();
  }

  // Mock avatar request for message2 from enqueuing the message.
  {
    base::RunLoop run_loop;
    auto quit_closure = run_loop.QuitClosure();

    EXPECT_CALL(*sharing_service(), GetAvatarImageForURL)
        .WillOnce([&](GURL url, int size, auto load_callback, auto fetcher) {
          EXPECT_EQ(
              url,
              GURL(message2.attributions.front().triggering_user->avatar_url));
          EXPECT_EQ(size, avatar_size);

          // Trigger callback with a mock avatar.
          std::move(load_callback).Run(favicon::GetDefaultFavicon());

          quit_closure.Run();
        });

    processor()->Enqueue(message2, callback2.Get());
    run_loop.Run();
  }

  EXPECT_TRUE(processor()->IsMessageShowing());
  EXPECT_EQ(2, processor()->GetQueueSize());

  EXPECT_CALL(callback2, Run(true));

  // Fast forward until toast has timed out.
  FastForwardByToastDuration();

  EXPECT_TRUE(processor()->IsMessageShowing());
  EXPECT_EQ(1, processor()->GetQueueSize());

  // Fast forward until toast has timed out.
  FastForwardByToastDuration();

  EXPECT_FALSE(processor()->IsMessageShowing());
  EXPECT_EQ(0, processor()->GetQueueSize());
}

}  // namespace tab_groups
