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
  attribution.triggering_user = member;
  attribution.tab_metadata = tab_metadata;
  attribution.tab_group_metadata = tab_group_metadata;

  InstantMessage message;
  message.attribution = attribution;
  message.collaboration_event = event;
  message.level = InstantNotificationLevel::BROWSER;
  message.type = event == CollaborationEvent::TAB_REMOVED
                     ? InstantNotificationType::CONFLICT_TAB_REMOVED
                     : InstantNotificationType::UNDEFINED;

  return message;
}

class TestInstantMessageQueueProcessor : public InstantMessageQueueProcessor {
 public:
  TestInstantMessageQueueProcessor() : InstantMessageQueueProcessor(nullptr) {}
  ~TestInstantMessageQueueProcessor() override = default;

  bool MaybeShowToastInBrowser(Browser* browser,
                               std::optional<ToastParams> params) override {
    return toast_will_be_shown_;
  }

  void SetToastWillBeShown(bool enabled) { toast_will_be_shown_ = enabled; }

  void FastForwardByToastDuration(
      base::test::TaskEnvironment* task_environment) {
    task_environment->FastForwardBy(GetMessageInterval());
  }

 private:
  bool toast_will_be_shown_ = false;
};

}  // namespace

class InstantMessageQueueProcessorTest : public testing::Test {
 protected:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(toast_features::kToastFramework);
    processor_ = std::make_unique<TestInstantMessageQueueProcessor>();
  }

  TestInstantMessageQueueProcessor* processor() { return processor_.get(); }

  void FastForwardByToastDuration() {
    task_environment_.FastForwardBy(processor()->GetMessageInterval());
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestInstantMessageQueueProcessor> processor_;
  base::test::ScopedFeatureList feature_list_;
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
  message.attribution.triggering_user = std::nullopt;

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

  auto message1 = CreateMessage(CollaborationEvent::COLLABORATION_MEMBER_ADDED);
  auto message2 = CreateMessage(CollaborationEvent::TAB_GROUP_REMOVED);
  base::MockCallback<SuccessCallback> callback1;
  base::MockCallback<SuccessCallback> callback2;

  // Message will succeed.
  EXPECT_CALL(callback1, Run(true));
  processor()->Enqueue(message1, callback1.Get());
  processor()->Enqueue(message2, callback2.Get());

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
