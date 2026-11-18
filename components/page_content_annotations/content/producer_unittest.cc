// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/content/producer.h"

#include <optional>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "content/public/browser/page.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace page_content_annotations {

namespace {

using ::testing::ElementsAre;
using ::testing::NiceMock;
using ::testing::StrictMock;

template <typename T>
class MockProducer : public Producer<T> {
 public:
  MOCK_METHOD(base::CallbackListSubscription,
              Subscribe,
              (ConsumerCallback<T>),
              (override));
  MOCK_METHOD(void, OnSubscribersChanged, (), (override));
};

using SubscriberRegistryTest = content::RenderViewHostTestHarness;

TEST_F(SubscriberRegistryTest, TracksDemandAndSubscriberCount) {
  StrictMock<MockProducer<int>> producer;
  SubscriberRegistry<int> registry(producer);
  EXPECT_FALSE(registry.HasSubscribers());
  EXPECT_EQ(0u, registry.GetSubscriberCount());

  EXPECT_CALL(producer, OnSubscribersChanged());
  std::optional<base::CallbackListSubscription> sub1 = registry.Subscribe(
      base::BindRepeating([](content::Page&, const int&) {}));
  EXPECT_TRUE(registry.HasSubscribers());
  EXPECT_EQ(1u, registry.GetSubscriberCount());

  EXPECT_CALL(producer, OnSubscribersChanged());
  std::optional<base::CallbackListSubscription> sub2 = registry.Subscribe(
      base::BindRepeating([](content::Page&, const int&) {}));
  EXPECT_TRUE(registry.HasSubscribers());
  EXPECT_EQ(2u, registry.GetSubscriberCount());

  EXPECT_CALL(producer, OnSubscribersChanged());
  sub1.reset();
  EXPECT_TRUE(registry.HasSubscribers());
  EXPECT_EQ(1u, registry.GetSubscriberCount());

  EXPECT_CALL(producer, OnSubscribersChanged());
  sub2.reset();
  EXPECT_FALSE(registry.HasSubscribers());
  EXPECT_EQ(0u, registry.GetSubscriberCount());
}

TEST_F(SubscriberRegistryTest,
       DestroyingRegistryWithActiveSubscriptionDoesNotNotifyProducer) {
  StrictMock<MockProducer<int>> producer;
  std::optional<base::CallbackListSubscription> sub;
  {
    std::optional<SubscriberRegistry<int>> registry(producer);
    EXPECT_CALL(producer, OnSubscribersChanged());
    sub = registry->Subscribe(
        base::BindRepeating([](content::Page&, const int&) {}));
    EXPECT_TRUE(registry->HasSubscribers());

    // Destroying `registry` while `sub` is still alive must not invoke
    // `OnSubscribersChanged()`.
    EXPECT_CALL(producer, OnSubscribersChanged()).Times(0);
    registry.reset();
  }

  // Resetting `sub` after `registry` was destroyed is also a safe no-op.
  sub.reset();
}

TEST_F(SubscriberRegistryTest,
       DecrementsCountForEachSubscriptionRemovedDuringNotify) {
  StrictMock<MockProducer<int>> producer;
  SubscriberRegistry<int> registry(producer);

  std::optional<base::CallbackListSubscription> sub1;
  std::optional<base::CallbackListSubscription> sub2;

  EXPECT_CALL(producer, OnSubscribersChanged()).Times(2);
  sub1 = registry.Subscribe(
      base::BindLambdaForTesting([&](content::Page&, const int&) {
        sub1.reset();
        sub2.reset();
      }));
  sub2 = registry.Subscribe(
      base::BindRepeating([](content::Page&, const int&) { ADD_FAILURE(); }));
  EXPECT_EQ(2u, registry.GetSubscriberCount());

  EXPECT_CALL(producer, OnSubscribersChanged()).Times(2);
  registry.Notify(web_contents()->GetPrimaryPage(), 42);
  EXPECT_FALSE(registry.HasSubscribers());
  EXPECT_EQ(0u, registry.GetSubscriberCount());
}

TEST_F(SubscriberRegistryTest, BroadcastsPayloadsToActiveSubscribers) {
  NiceMock<MockProducer<std::string>> producer;
  SubscriberRegistry<std::string> registry(producer);

  std::vector<std::string> sub1_received;
  std::vector<std::string> sub2_received;

  std::optional<base::CallbackListSubscription> sub1 =
      registry.Subscribe(base::BindLambdaForTesting(
          [&](content::Page& page, const std::string& payload) {
            EXPECT_EQ(&web_contents()->GetPrimaryPage(), &page);
            sub1_received.push_back(payload);
          }));
  std::optional<base::CallbackListSubscription> sub2 =
      registry.Subscribe(base::BindLambdaForTesting(
          [&](content::Page& page, const std::string& payload) {
            EXPECT_EQ(&web_contents()->GetPrimaryPage(), &page);
            sub2_received.push_back(payload);
          }));

  registry.Notify(web_contents()->GetPrimaryPage(), "first");
  EXPECT_THAT(sub1_received, ElementsAre("first"));
  EXPECT_THAT(sub2_received, ElementsAre("first"));

  sub1.reset();
  registry.Notify(web_contents()->GetPrimaryPage(), "second");
  EXPECT_THAT(sub1_received, ElementsAre("first"));
  EXPECT_THAT(sub2_received, ElementsAre("first", "second"));
}

}  // namespace

}  // namespace page_content_annotations
