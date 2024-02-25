// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/notification_processor.h"
#include <string>

#include "ash/constants/ash_features.h"
#include "base/memory/ptr_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/phonehub/fake_notification_manager.h"
#include "chromeos/ash/components/phonehub/phone_model_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace ash {
namespace phonehub {

namespace {

constexpr int64_t kNotificationIdA = 1;
constexpr int64_t kNotificationIdB = 2;
constexpr int64_t kNotificationIdC = 3;

constexpr int64_t kInlineReplyIdA = 3;
constexpr int64_t kInlineReplyIdB = 4;

constexpr int64_t kOpenableActionId = -2;
constexpr int64_t kAnswerActionId = 1;
constexpr int64_t kDeclineActionId = 2;
constexpr int64_t kHangupActionId = 3;

const char kIconDataA[] = "icon_a";
const char kIconDataB[] = "icon_b";

const char kSharedImageA[] = "shared_image_a";
const char kSharedImageB[] = "shared_image_b";

const char kContactImageA[] = "contact_image_a";
const char kContactImageB[] = "contact_image_b";

// Garbage color for the purpose of verification in these tests.
const SkColor kIconColor = SkColorSetRGB(0x12, 0x34, 0x56);

SkBitmap TestBitmap() {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(1, 1);
  return bitmap;
}

gfx::Image TestImage() {
  gfx::ImageSkia image_skia = gfx::ImageSkia::CreateFrom1xBitmap(TestBitmap());
  image_skia.MakeThreadSafe();
  return gfx::Image(image_skia);
}

}  // namespace

class NotificationProcessorTest : public testing::Test {
 public:
  friend class NotificationProcessor;

  NotificationProcessorTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kEcheSWA,
                              features::kPhoneHubCallNotification},
        /*disabled_features=*/{});
  }
  NotificationProcessorTest(const NotificationProcessorTest&) = delete;
  NotificationProcessorTest& operator=(const NotificationProcessorTest&) =
      delete;
  ~NotificationProcessorTest() override = default;

  class FakeImageDecoderDelegate
      : public NotificationProcessor::ImageDecoderDelegate {
   public:
    using DecodeImageCallback = NotificationProcessor::DecodeImageCallback;

    FakeImageDecoderDelegate() = default;
    ~FakeImageDecoderDelegate() override = default;

    void PerformImageDecode(
        const std::string& data,
        DecodeImageCallback single_image_decoded_closure) override {
      decode_image_callbacks_.push(std::move(single_image_decoded_closure));
    }

    size_t NumberOfDecodeImageCallbacks() {
      return decode_image_callbacks_.size();
    }

    void RunNextCallback(SkBitmap bitmap) {
      std::move(decode_image_callbacks_.front()).Run(bitmap);
      decode_image_callbacks_.pop();
    }

    void RunAllCallbacks() {
      while (!decode_image_callbacks_.empty())
        RunNextCallback(TestBitmap());
    }

    void RunAllCallbacksWithEmpty() {
      while (!decode_image_callbacks_.empty())
        RunNextCallback(SkBitmap());
    }

    std::queue<DecodeImageCallback> decode_image_callbacks_;
  };

  // testing::Test:
  void SetUp() override {
    fake_notification_manager_ = std::make_unique<FakeNotificationManager>();

    notification_processor_ = base::WrapUnique(new NotificationProcessor(
        fake_notification_manager_.get(),
        std::make_unique<FakeImageDecoderDelegate>()));
  }

  FakeImageDecoderDelegate* image_decoder_delegate() {
    return static_cast<FakeImageDecoderDelegate*>(
        notification_processor_->delegate_.get());
  }

  NotificationProcessor* notification_processor() {
    return notification_processor_.get();
  }

  FakeNotificationManager* fake_notification_manager() {
    return fake_notification_manager_.get();
  }

  size_t NumPendingRequests() {
    return notification_processor()->pending_notification_requests_.size();
  }

  proto::Notification CreateNewInlineReplyableOpenableNotification(
      int64_t notification_id,
      int64_t inline_reply_id,
      Notification::InteractionBehavior behavior) {
    return CreateNewInlineReplyableNotification(
        notification_id, inline_reply_id,
        /* icon= */ std::string(),
        /* shared_image= */ std::string(),
        /* contact_image= */ std::string(), behavior);
  }

  proto::Notification CreateNewInlineReplyableNotification(
      int64_t notification_id,
      int64_t inline_reply_id,
      std::string icon = std::string(),
      std::string shared_image = std::string(),
      std::string contact_image = std::string(),
      Notification::InteractionBehavior behavior =
          Notification::InteractionBehavior::kNone,
      proto::AppStreamabilityStatus app_streamability_status =
          proto::AppStreamabilityStatus::STREAMABLE) {
    auto origin_app = std::make_unique<proto::App>();
    origin_app->set_icon(icon);
    origin_app->set_app_streamability_status(app_streamability_status);

    proto::Notification notification;
    notification.set_id(notification_id);
    notification.set_allocated_origin_app(origin_app.release());
    notification.set_contact_image(contact_image);
    notification.set_shared_image(shared_image);
    notification.set_category(
        proto::Notification::Category::Notification_Category_CONVERSATION);

    notification.add_actions();
    proto::Action* mutable_action = notification.mutable_actions(0);
    mutable_action->set_id(inline_reply_id);
    mutable_action->set_type(proto::Action_InputType::Action_InputType_TEXT);

    if (behavior == Notification::InteractionBehavior::kOpenable) {
      notification.add_actions();
      proto::Action* open_action = notification.mutable_actions(1);
      open_action->set_id(kOpenableActionId);
      open_action->set_type(proto::Action_InputType::Action_InputType_OPEN);
    }
    return notification;
  }

  proto::Notification CreateNewInlineReplyableMonochromeIconNotification(
      int64_t notification_id,
      int64_t inline_reply_id,
      std::optional<SkColor> icon_color = std::nullopt,
      std::string icon = std::string()) {
    proto::Notification notification = CreateNewInlineReplyableNotification(
        notification_id, inline_reply_id, icon);
    proto::App* origin_app = notification.mutable_origin_app();
    origin_app->set_monochrome_icon_mask(icon);
    if (icon_color != std::nullopt) {
      auto color_rgb = std::make_unique<proto::ColorRgb>();
      color_rgb->set_red(SkColorGetR(*icon_color));
      color_rgb->set_green(SkColorGetG(*icon_color));
      color_rgb->set_blue(SkColorGetB(*icon_color));
      origin_app->set_allocated_monochrome_icon_color(color_rgb.release());
    }
    return notification;
  }

  proto::Notification CreateNonTextTypeNotification(
      int64_t notification_id,
      int64_t inline_reply_id,
      std::string icon = std::string()) {
    auto origin_app = std::make_unique<proto::App>();
    origin_app->set_icon(icon);

    proto::Notification notification;
    notification.set_id(notification_id);
    notification.set_allocated_origin_app(origin_app.release());

    notification.add_actions();
    proto::Action* open_action = notification.mutable_actions(0);
    open_action->set_id(kOpenableActionId);
    open_action->set_type(proto::Action_InputType::Action_InputType_OPEN);

    return notification;
  }

  proto::Notification CreateIncomingCallNotification(int64_t notification_id) {
    auto origin_app = std::make_unique<proto::App>();
    origin_app->set_icon(std::string());

    proto::Notification notification;
    notification.set_id(notification_id);
    notification.set_allocated_origin_app(origin_app.release());
    notification.set_contact_image(std::string());
    notification.set_shared_image(std::string());
    notification.set_category(
        proto::Notification::Category::Notification_Category_INCOMING_CALL);

    notification.add_actions();
    proto::Action* answer_action = notification.mutable_actions(0);
    answer_action->set_id(kAnswerActionId);
    answer_action->set_call_action(
        proto::Action_CallAction::Action_CallAction_ANSWER);

    notification.add_actions();
    proto::Action* decline_action = notification.mutable_actions(1);
    decline_action->set_id(kDeclineActionId);
    decline_action->set_call_action(
        proto::Action_CallAction::Action_CallAction_DECLINE);

    return notification;
  }

  proto::Notification CreateOngoingCallNotification(int64_t notification_id) {
    auto origin_app = std::make_unique<proto::App>();
    origin_app->set_icon(std::string());

    proto::Notification notification;
    notification.set_id(notification_id);
    notification.set_allocated_origin_app(origin_app.release());
    notification.set_contact_image(std::string());
    notification.set_shared_image(std::string());
    notification.set_category(
        proto::Notification::Category::Notification_Category_ONGOING_CALL);

    notification.add_actions();
    proto::Action* hangup_action = notification.mutable_actions(0);
    hangup_action->set_id(kHangupActionId);
    hangup_action->set_call_action(
        proto::Action_CallAction::Action_CallAction_HANGUP);

    notification.add_actions();
    proto::Action* open_action = notification.mutable_actions(1);
    open_action->set_id(kOpenableActionId);
    open_action->set_type(proto::Action_InputType::Action_InputType_OPEN);

    return notification;
  }

 private:
  std::unique_ptr<FakeNotificationManager> fake_notification_manager_;
  std::unique_ptr<NotificationProcessor> notification_processor_;

  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(NotificationProcessorTest, FailedToDecodeImage) {
  std::vector<proto::Notification> first_set_of_notifications;

  // The icon should be an empty image as the decoder failed to decode the
  // image to be used as both the color_icon and monochrome_icon_mask.
  first_set_of_notifications.emplace_back(CreateNewInlineReplyableNotification(
      kNotificationIdA, kInlineReplyIdA, kIconDataA));
  notification_processor()->AddNotifications(first_set_of_notifications);
  image_decoder_delegate()->RunNextCallback(SkBitmap());
  image_decoder_delegate()->RunNextCallback(SkBitmap());

  const Notification* notification =
      fake_notification_manager()->GetNotification(kNotificationIdA);
  EXPECT_TRUE(notification->app_metadata().color_icon.IsEmpty());
  EXPECT_TRUE(notification->app_metadata().monochrome_icon_mask.has_value());
  EXPECT_FALSE(notification->shared_image().has_value());
  EXPECT_FALSE(notification->contact_image().has_value());
}

TEST_F(NotificationProcessorTest, ShouldSkipDecodeImageIfNotAvailable) {
  std::vector<proto::Notification> first_set_of_notifications;

  first_set_of_notifications.emplace_back(CreateNonTextTypeNotification(
      kNotificationIdA, kInlineReplyIdA, kIconDataA));
  notification_processor()->AddNotifications(first_set_of_notifications);

  EXPECT_EQ(0u, image_decoder_delegate()->NumberOfDecodeImageCallbacks());
  image_decoder_delegate()->RunAllCallbacks();

  EXPECT_EQ(0u, fake_notification_manager()->num_notifications());
}

TEST_F(NotificationProcessorTest, MonochromeIconFieldsPopulatedCorrectly) {
  std::vector<proto::Notification> first_set_of_notifications;

  // Legacy notifications don't supply color and should not be filled in.
  first_set_of_notifications.emplace_back(CreateNewInlineReplyableNotification(
      kNotificationIdA, kInlineReplyIdA, kIconDataA));
  notification_processor()->AddNotifications(first_set_of_notifications);
  image_decoder_delegate()->RunAllCallbacks();

  const Notification* notification =
      fake_notification_manager()->GetNotification(kNotificationIdA);
  EXPECT_TRUE(gfx::test::AreImagesEqual(notification->app_metadata().color_icon,
                                        TestImage()));
  EXPECT_FALSE(notification->app_metadata().icon_color.has_value());

  // Monochrome notifications without a color should not have icon_color filled.
  first_set_of_notifications.clear();
  first_set_of_notifications.emplace_back(
      CreateNewInlineReplyableMonochromeIconNotification(
          kNotificationIdA, kInlineReplyIdA, std::nullopt, kIconDataA));
  notification_processor()->AddNotifications(first_set_of_notifications);
  image_decoder_delegate()->RunAllCallbacks();

  notification = fake_notification_manager()->GetNotification(kNotificationIdA);
  EXPECT_TRUE(notification->app_metadata().icon_is_monochrome);
  EXPECT_TRUE(gfx::test::AreImagesEqual(notification->app_metadata().color_icon,
                                        TestImage()));
  EXPECT_FALSE(notification->app_metadata().icon_color.has_value());

  // Monochrome notifications with a color should have icon_color filled.
  first_set_of_notifications.clear();
  first_set_of_notifications.emplace_back(
      CreateNewInlineReplyableMonochromeIconNotification(
          kNotificationIdA, kInlineReplyIdA, kIconColor, kIconDataA));
  notification_processor()->AddNotifications(first_set_of_notifications);
  image_decoder_delegate()->RunAllCallbacks();

  notification = fake_notification_manager()->GetNotification(kNotificationIdA);
  EXPECT_TRUE(notification->app_metadata().icon_is_monochrome);
  EXPECT_TRUE(gfx::test::AreImagesEqual(notification->app_metadata().color_icon,
                                        TestImage()));
  EXPECT_TRUE(notification->app_metadata().icon_color.has_value());
  EXPECT_TRUE(*notification->app_metadata().icon_color == kIconColor);

  // Monochrome notifications without an icon should not fill in color.
  first_set_of_notifications.clear();
  first_set_of_notifications.emplace_back(
      CreateNewInlineReplyableMonochromeIconNotification(
          kNotificationIdA, kInlineReplyIdA, kIconColor,
          /*icon=*/std::string()));
  notification_processor()->AddNotifications(first_set_of_notifications);
  image_decoder_delegate()->RunAllCallbacksWithEmpty();

  notification = fake_notification_manager()->GetNotification(kNotificationIdA);
  EXPECT_TRUE(notification->app_metadata().icon_is_monochrome);
  EXPECT_TRUE(notification->app_metadata().color_icon.IsEmpty());
  EXPECT_FALSE(notification->app_metadata().icon_color.has_value());
}

TEST_F(NotificationProcessorTest, ImageFieldPopulatedCorrectly) {
  std::vector<proto::Notification> first_set_of_notifications;

  // The icon should be populated. The shared and contact image should be null.
  first_set_of_notifications.emplace_back(CreateNewInlineReplyableNotification(
      kNotificationIdA, kInlineReplyIdA, kIconDataA));
  notification_processor()->AddNotifications(first_set_of_notifications);
  image_decoder_delegate()->RunAllCallbacks();

  const Notification* notification =
      fake_notification_manager()->GetNotification(kNotificationIdA);
  EXPECT_TRUE(gfx::test::AreImagesEqual(notification->app_metadata().color_icon,
                                        TestImage()));
  EXPECT_FALSE(notification->shared_image().has_value());
  EXPECT_FALSE(notification->contact_image().has_value());

  // The icon and shared image should be populated. The contact image should be
  // null.
  first_set_of_notifications.clear();
  first_set_of_notifications.emplace_back(CreateNewInlineReplyableNotification(
      kNotificationIdA, kInlineReplyIdA, kIconDataA, kSharedImageA));
  notification_processor()->AddNotifications(first_set_of_notifications);
  image_decoder_delegate()->RunAllCallbacks();

  notification = fake_notification_manager()->GetNotification(kNotificationIdA);
  EXPECT_TRUE(gfx::test::AreImagesEqual(notification->app_metadata().color_icon,
                                        TestImage()));
  EXPECT_TRUE(
      gfx::test::AreImagesEqual(*notification->shared_image(), TestImage()));
  EXPECT_FALSE(notification->contact_image().has_value());

  // The icon and contact image should be populated. The shared image should be
  // null.
  first_set_of_notifications.clear();
  first_set_of_notifications.emplace_back(CreateNewInlineReplyableNotification(
      kNotificationIdA, kInlineReplyIdA, kIconDataA, std::string(),
      kContactImageA));
  notification_processor()->AddNotifications(first_set_of_notifications);
  image_decoder_delegate()->RunAllCallbacks();

  notification = fake_notification_manager()->GetNotification(kNotificationIdA);
  EXPECT_TRUE(gfx::test::AreImagesEqual(notification->app_metadata().color_icon,
                                        TestImage()));
  EXPECT_FALSE(notification->shared_image().has_value());
  EXPECT_TRUE(
      gfx::test::AreImagesEqual(*notification->contact_image(), TestImage()));

  // All images should be should be populated.
  first_set_of_notifications.clear();
  first_set_of_notifications.emplace_back(CreateNewInlineReplyableNotification(
      kNotificationIdA, kInlineReplyIdA, kIconDataA, kSharedImageA,
      kContactImageA));
  notification_processor()->AddNotifications(first_set_of_notifications);
  image_decoder_delegate()->RunAllCallbacks();
  EXPECT_TRUE(gfx::test::AreImagesEqual(notification->app_metadata().color_icon,
                                        TestImage()));
  EXPECT_TRUE(
      gfx::test::AreImagesEqual(*notification->shared_image(), TestImage()));
  EXPECT_TRUE(
      gfx::test::AreImagesEqual(*notification->contact_image(), TestImage()));
}

TEST_F(NotificationProcessorTest, StreamabilityStatus) {
  std::vector<proto::Notification> first_set_of_notifications;

  first_set_of_notifications.emplace_back(CreateNewInlineReplyableNotification(
      kNotificationIdA, kInlineReplyIdA, kIconDataA, std::string(),
      std::string(), Notification::InteractionBehavior::kNone,
      proto::AppStreamabilityStatus::BLOCK_LISTED));
  notification_processor()->AddNotifications(first_set_of_notifications);
  image_decoder_delegate()->RunAllCallbacks();

  const Notification* notification =
      fake_notification_manager()->GetNotification(kNotificationIdA);
  EXPECT_EQ(proto::AppStreamabilityStatus::BLOCK_LISTED,
            notification->app_metadata().app_streamability_status);
}

TEST_F(NotificationProcessorTest, AddRemoveClearWithoutRace) {
  // Add 2 notifications with all images populated.
  std::vector<proto::Notification> first_set_of_notifications;
  first_set_of_notifications.emplace_back(CreateNewInlineReplyableNotification(
      kNotificationIdA, kInlineReplyIdA, kIconDataA, kSharedImageA,
      kContactImageA));
  first_set_of_notifications.emplace_back(CreateNewInlineReplyableNotification(
      kNotificationIdB, kInlineReplyIdB, kIconDataB, kSharedImageB,
      kContactImageB));

  notification_processor()->AddNotifications(first_set_of_notifications);

  // 8 image decode callbacks will occur for kIconDataA, data for icons twice
  // for each notification, kSharedImageA, kContactImageA, kIconDataB,
  // kSharedImageB, and kContactImageB.
  EXPECT_EQ(8u, image_decoder_delegate()->NumberOfDecodeImageCallbacks());
  image_decoder_delegate()->RunAllCallbacks();

  EXPECT_EQ(2u, fake_notification_manager()->num_notifications());
  EXPECT_TRUE(fake_notification_manager()->GetNotification(kNotificationIdA));
  EXPECT_TRUE(fake_notification_manager()->GetNotification(kNotificationIdB));

  // Remove notification with id kNotificationIdA.
  base::flat_set<int64_t> ids_of_notifications_to_remove;
  ids_of_notifications_to_remove.emplace(kNotificationIdA);
  notification_processor()->RemoveNotifications(ids_of_notifications_to_remove);
  EXPECT_EQ(1u, fake_notification_manager()->num_notifications());
  EXPECT_FALSE(fake_notification_manager()->GetNotification(kNotificationIdA));
  EXPECT_TRUE(fake_notification_manager()->GetNotification(kNotificationIdB));

  // Clear all notifications.
  notification_processor()->ClearNotificationsAndPendingUpdates();
  EXPECT_EQ(0u, fake_notification_manager()->num_notifications());
}

TEST_F(NotificationProcessorTest, AddRemoveWithRace) {
  // Add 2 notifications.
  std::vector<proto::Notification> first_set_of_notifications;
  first_set_of_notifications.emplace_back(CreateNewInlineReplyableNotification(
      kNotificationIdA, kInlineReplyIdA, kIconDataA, kSharedImageA));
  first_set_of_notifications.emplace_back(CreateNewInlineReplyableNotification(
      kNotificationIdB, kInlineReplyIdB, kIconDataB));

  notification_processor()->AddNotifications(first_set_of_notifications);

  // One pending requests because |first_set_of_notifications| processing
  // occurred immediately.
  EXPECT_EQ(1u, NumPendingRequests());

  // Remove notification with id kNotificationIdA while
  // |first_set_of_notifications| is still being processed.
  base::flat_set<int64_t> ids_of_notifications_to_remove;
  ids_of_notifications_to_remove.emplace(kNotificationIdA);
  notification_processor()->RemoveNotifications(ids_of_notifications_to_remove);

  // Pending delete request, first in the queue.
  EXPECT_EQ(2u, NumPendingRequests());
  EXPECT_EQ(0u, fake_notification_manager()->num_notifications());

  // Add a set of notifications such that only one image needs to be decoded,
  // when neither the first set has completed processing more the remove request
  // has been fully processed.
  std::vector<proto::Notification> second_set_of_notifications;
  second_set_of_notifications.emplace_back(CreateNewInlineReplyableNotification(
      kNotificationIdA, kInlineReplyIdA, kIconDataA));
  notification_processor()->AddNotifications(second_set_of_notifications);

  // Pending add request, second in the queue.
  EXPECT_EQ(3u, NumPendingRequests());
  EXPECT_EQ(0u, fake_notification_manager()->num_notifications());

  // 5 image decode callbacks will occur (two icons, two monochrome icons, and a
  // shared image). When the last image decode callback is finished running,
  // which in this case is original icon2, it will cause the next notification
  // edit request to be executed.
  EXPECT_EQ(5u, image_decoder_delegate()->NumberOfDecodeImageCallbacks());
  EXPECT_EQ(3u, NumPendingRequests());
  image_decoder_delegate()->RunNextCallback(TestBitmap());

  EXPECT_EQ(4u, image_decoder_delegate()->NumberOfDecodeImageCallbacks());
  EXPECT_EQ(3u, NumPendingRequests());
  image_decoder_delegate()->RunNextCallback(TestBitmap());

  EXPECT_EQ(3u, image_decoder_delegate()->NumberOfDecodeImageCallbacks());
  EXPECT_EQ(3u, NumPendingRequests());

  // The scheduled remove callback will occur, then subsequently the add
  // notification with 1 image.
  image_decoder_delegate()->RunNextCallback(TestBitmap());
  image_decoder_delegate()->RunNextCallback(TestBitmap());
  image_decoder_delegate()->RunNextCallback(TestBitmap());
  EXPECT_EQ(1u, NumPendingRequests());
  EXPECT_EQ(2u, image_decoder_delegate()->NumberOfDecodeImageCallbacks());

  EXPECT_EQ(1u, fake_notification_manager()->num_notifications());
  EXPECT_FALSE(fake_notification_manager()->GetNotification(kNotificationIdA));
  EXPECT_TRUE(fake_notification_manager()->GetNotification(kNotificationIdB));

  // 1 image decode callback will occur.
  image_decoder_delegate()->RunAllCallbacks();
  EXPECT_EQ(0u, NumPendingRequests());
  EXPECT_EQ(2u, fake_notification_manager()->num_notifications());
  EXPECT_TRUE(fake_notification_manager()->GetNotification(kNotificationIdA));
  EXPECT_TRUE(fake_notification_manager()->GetNotification(kNotificationIdB));
}

TEST_F(NotificationProcessorTest, AddClearAllWithRace) {
  std::vector<proto::Notification> first_set_of_notifications;
  first_set_of_notifications.emplace_back(CreateNewInlineReplyableNotification(
      kNotificationIdA, kInlineReplyIdA, kIconDataA, kSharedImageA));
  first_set_of_notifications.emplace_back(CreateNewInlineReplyableNotification(
      kNotificationIdB, kInlineReplyIdB, kIconDataA));

  notification_processor()->AddNotifications(first_set_of_notifications);

  // Clearing Notifications will invalidate all callbacks in process and
  // immediately clear all pointers.
  notification_processor()->ClearNotificationsAndPendingUpdates();
  EXPECT_EQ(0u, fake_notification_manager()->num_notifications());
  image_decoder_delegate()->RunAllCallbacks();
  EXPECT_EQ(0u, NumPendingRequests());
  EXPECT_EQ(0u, fake_notification_manager()->num_notifications());
  EXPECT_FALSE(fake_notification_manager()->GetNotification(kNotificationIdA));
  EXPECT_FALSE(fake_notification_manager()->GetNotification(kNotificationIdB));
}

TEST_F(NotificationProcessorTest, InteractionBehaviorPopulatedCorrectly) {
  std::vector<proto::Notification> first_set_of_notifications;

  // The notification should be openable if a OPEN action is specified.
  first_set_of_notifications.emplace_back(
      CreateNewInlineReplyableOpenableNotification(
          kNotificationIdA, kInlineReplyIdA,
          Notification::InteractionBehavior::kOpenable));
  notification_processor()->AddNotifications(first_set_of_notifications);
  image_decoder_delegate()->RunAllCallbacks();

  const Notification* notification =
      fake_notification_manager()->GetNotification(kNotificationIdA);
  EXPECT_EQ(Notification::InteractionBehavior::kOpenable,
            notification->interaction_behavior());
  EXPECT_EQ(Notification::Category::kConversation, notification->category());

  // The notification should not specify interaction behaviors if none are
  // available.
  first_set_of_notifications.clear();
  first_set_of_notifications.emplace_back(
      CreateNewInlineReplyableNotification(kNotificationIdA, kInlineReplyIdA));
  notification_processor()->AddNotifications(first_set_of_notifications);
  image_decoder_delegate()->RunAllCallbacks();

  notification = fake_notification_manager()->GetNotification(kNotificationIdA);
  EXPECT_EQ(Notification::InteractionBehavior::kNone,
            notification->interaction_behavior());
  EXPECT_EQ(Notification::Category::kConversation, notification->category());

  // The notification has IncomingCall category if answer action is
  // found.
  first_set_of_notifications.clear();
  first_set_of_notifications.emplace_back(
      CreateIncomingCallNotification(kNotificationIdA));
  notification_processor()->AddNotifications(first_set_of_notifications);
  image_decoder_delegate()->RunAllCallbacks();

  notification = fake_notification_manager()->GetNotification(kNotificationIdA);
  EXPECT_EQ(Notification::Category::kIncomingCall, notification->category());

  // The notification has OngoingCall category if hangup action is
  // found.
  first_set_of_notifications.clear();
  first_set_of_notifications.emplace_back(
      CreateOngoingCallNotification(kNotificationIdC));
  notification_processor()->AddNotifications(first_set_of_notifications);
  image_decoder_delegate()->RunAllCallbacks();

  notification = fake_notification_manager()->GetNotification(kNotificationIdC);
  EXPECT_EQ(Notification::Category::kOngoingCall, notification->category());
}

TEST_F(NotificationProcessorTest, ActionIdMapPopulatedCorrectly) {
  std::vector<proto::Notification> first_set_of_notifications;

  // The inline reply notification should have reply action id.
  first_set_of_notifications.emplace_back(
      CreateNewInlineReplyableNotification(kNotificationIdA, kInlineReplyIdA));
  notification_processor()->AddNotifications(first_set_of_notifications);
  image_decoder_delegate()->RunAllCallbacks();

  const Notification* notification =
      fake_notification_manager()->GetNotification(kNotificationIdA);
  EXPECT_EQ(1u, notification->action_id_map().size());
  EXPECT_EQ(kInlineReplyIdA, notification->action_id_map().at(
                                 Notification::ActionType::kInlineReply));

  // The incoming call notification should have answer and decline action ids.
  first_set_of_notifications.clear();
  first_set_of_notifications.emplace_back(
      CreateIncomingCallNotification(kNotificationIdA));
  notification_processor()->AddNotifications(first_set_of_notifications);
  image_decoder_delegate()->RunAllCallbacks();

  notification = fake_notification_manager()->GetNotification(kNotificationIdA);
  EXPECT_EQ(2u, notification->action_id_map().size());
  EXPECT_EQ(kAnswerActionId, notification->action_id_map().at(
                                 Notification::ActionType::kAnswer));
  EXPECT_EQ(kDeclineActionId, notification->action_id_map().at(
                                  Notification::ActionType::kDecline));

  // The ongoing call notification should have hangup action id.
  first_set_of_notifications.clear();
  first_set_of_notifications.emplace_back(
      CreateOngoingCallNotification(kNotificationIdC));
  notification_processor()->AddNotifications(first_set_of_notifications);
  image_decoder_delegate()->RunAllCallbacks();

  notification = fake_notification_manager()->GetNotification(kNotificationIdC);
  EXPECT_EQ(1u, notification->action_id_map().size());
  EXPECT_EQ(kHangupActionId, notification->action_id_map().at(
                                 Notification::ActionType::kHangup));
}

}  // namespace phonehub
}  // namespace ash
