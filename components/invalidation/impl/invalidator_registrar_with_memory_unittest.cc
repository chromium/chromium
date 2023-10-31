// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/invalidator_registrar_with_memory.h"

#include <memory>

#include "base/json/json_reader.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "components/invalidation/impl/fake_invalidation_handler.h"
#include "components/invalidation/public/invalidation.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/invalidation/public/invalidator_state.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::IsEmpty;
using testing::Pair;
using testing::UnorderedElementsAre;

namespace invalidation {

namespace {

template <class... Inv>
void Dispatch(InvalidatorRegistrarWithMemory& registrar, Inv... inv) {
  (registrar.DispatchInvalidationToHandlers(inv), ...);
}

template <class... Inv>
std::map<Topic, Invalidation> ExpectedInvalidations(Inv... inv) {
  std::map<Topic, Invalidation> expected_invalidations;
  (expected_invalidations.emplace(inv.topic(), inv), ...);
  return expected_invalidations;
}

constexpr char kTopicsToHandler[] = "invalidation.per_sender_topics_to_handler";

class InvalidatorRegistrarWithMemoryTest : public testing::Test {
 protected:
  const TopicData kTopic1 = {/*name=*/"topic_1", /*is_public=*/false};
  const TopicData kTopic2 = {/*name=*/"topic_2", /*is_public=*/false};
  const TopicData kTopic3 = {/*name=*/"topic_3", /*is_public=*/false};
  const TopicData kTopic4 = {/*name=*/"topic_4", /*is_public=*/false};
  const Invalidation kInv1 = Invalidation(kTopic1.name, 1, "1");
  const Invalidation kInv2 = Invalidation(kTopic2.name, 2, "2");
  const Invalidation kInv3 = Invalidation(kTopic3.name, 3, "3");
  const Invalidation kInv4 = Invalidation(kTopic4.name, 4, "4");
};

// Initialize the invalidator, register a handler, register some topics for that
// handler, and then unregister the handler, dispatching invalidations in
// between. The handler should only see invalidations when it's registered and
// its topics are registered.
TEST_F(InvalidatorRegistrarWithMemoryTest, Basic) {
  TestingPrefServiceSimple pref_service;
  InvalidatorRegistrarWithMemory::RegisterProfilePrefs(pref_service.registry());

  auto invalidator = std::make_unique<InvalidatorRegistrarWithMemory>(
      &pref_service, "sender_id");

  FakeInvalidationHandler handler("owner");
  invalidator->AddObserver(&handler);
  EXPECT_TRUE(invalidator->HasObserver(&handler));

  // Should be ignored since no topics are registered to |handler|.
  Dispatch(*invalidator, kInv1, kInv2, kInv3);
  EXPECT_EQ(0, handler.GetInvalidationCount());

  EXPECT_TRUE(
      invalidator->UpdateRegisteredTopics(&handler, {kTopic1, kTopic2}));

  invalidator->UpdateInvalidatorState(INVALIDATIONS_ENABLED);
  EXPECT_EQ(INVALIDATIONS_ENABLED, handler.GetInvalidatorState());

  Dispatch(*invalidator, kInv1, kInv2, kInv3);
  EXPECT_EQ(2, handler.GetInvalidationCount());
  EXPECT_EQ(ExpectedInvalidations(kInv1, kInv2),
            handler.GetReceivedInvalidations());
  handler.ClearReceivedInvalidations();

  // Remove kTopic1, add kTopic3.
  EXPECT_TRUE(
      invalidator->UpdateRegisteredTopics(&handler, {kTopic2, kTopic3}));

  // Removed topic should not be notified, newly-added ones should.
  Dispatch(*invalidator, kInv1, kInv2, kInv3);
  EXPECT_EQ(2, handler.GetInvalidationCount());
  EXPECT_EQ(ExpectedInvalidations(kInv2, kInv3),
            handler.GetReceivedInvalidations());
  handler.ClearReceivedInvalidations();

  invalidator->UpdateInvalidatorState(TRANSIENT_INVALIDATION_ERROR);
  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, handler.GetInvalidatorState());

  invalidator->UpdateInvalidatorState(INVALIDATION_CREDENTIALS_REJECTED);
  EXPECT_EQ(INVALIDATION_CREDENTIALS_REJECTED, handler.GetInvalidatorState());

  invalidator->RemoveObserver(&handler);
  EXPECT_FALSE(invalidator->HasObserver(&handler));

  // Should be ignored since |handler| isn't registered anymore.
  Dispatch(*invalidator, kInv1, kInv2, kInv3);
  EXPECT_EQ(0, handler.GetInvalidationCount());
}

// Register handlers and some topics for those handlers, register a handler with
// no topics, and register a handler with some topics but unregister it. Then,
// dispatch some invalidations. Handlers that are not registered should not get
// invalidations, and the ones that have registered topics should receive
// invalidations for those topics.
TEST_F(InvalidatorRegistrarWithMemoryTest, MultipleHandlers) {
  TestingPrefServiceSimple pref_service;
  InvalidatorRegistrarWithMemory::RegisterProfilePrefs(pref_service.registry());

  auto invalidator = std::make_unique<InvalidatorRegistrarWithMemory>(
      &pref_service, "sender_id");

  FakeInvalidationHandler handler1("owner_1");
  FakeInvalidationHandler handler2("owner_2");
  FakeInvalidationHandler handler3("owner_3");
  FakeInvalidationHandler handler4("owner_4");

  invalidator->AddObserver(&handler1);
  invalidator->AddObserver(&handler2);
  invalidator->AddObserver(&handler3);
  invalidator->AddObserver(&handler4);

  EXPECT_TRUE(
      invalidator->UpdateRegisteredTopics(&handler1, {kTopic1, kTopic2}));
  EXPECT_TRUE(invalidator->UpdateRegisteredTopics(&handler2, {kTopic3}));
  // Don't register any IDs for handler3.
  EXPECT_TRUE(invalidator->UpdateRegisteredTopics(&handler4, {kTopic4}));

  invalidator->RemoveObserver(&handler4);

  invalidator->UpdateInvalidatorState(INVALIDATIONS_ENABLED);
  EXPECT_EQ(INVALIDATIONS_ENABLED, handler1.GetInvalidatorState());
  EXPECT_EQ(INVALIDATIONS_ENABLED, handler2.GetInvalidatorState());
  EXPECT_EQ(INVALIDATIONS_ENABLED, handler3.GetInvalidatorState());
  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, handler4.GetInvalidatorState());

  Dispatch(*invalidator, kInv1, kInv2, kInv3, kInv4);

  EXPECT_EQ(2, handler1.GetInvalidationCount());
  EXPECT_EQ(ExpectedInvalidations(kInv1, kInv2),
            handler1.GetReceivedInvalidations());

  std::map<Topic, Invalidation> expected_invalidations2;
  expected_invalidations2.emplace(kInv3.topic(), kInv3);

  EXPECT_EQ(1, handler2.GetInvalidationCount());
  EXPECT_EQ(ExpectedInvalidations(kInv3), handler2.GetReceivedInvalidations());

  EXPECT_EQ(0, handler3.GetInvalidationCount());
  EXPECT_EQ(0, handler4.GetInvalidationCount());

  invalidator->UpdateInvalidatorState(TRANSIENT_INVALIDATION_ERROR);
  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, handler1.GetInvalidatorState());
  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, handler2.GetInvalidatorState());
  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, handler3.GetInvalidatorState());
  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, handler4.GetInvalidatorState());

  invalidator->RemoveObserver(&handler3);
  invalidator->RemoveObserver(&handler2);
  invalidator->RemoveObserver(&handler1);
}

// Multiple registrations by different handlers on the same topic should
// return false.
TEST_F(InvalidatorRegistrarWithMemoryTest, MultipleRegistrations) {
  TestingPrefServiceSimple pref_service;
  InvalidatorRegistrarWithMemory::RegisterProfilePrefs(pref_service.registry());

  auto invalidator = std::make_unique<InvalidatorRegistrarWithMemory>(
      &pref_service, "sender_id");

  FakeInvalidationHandler handler1("owner1");
  FakeInvalidationHandler handler2("owner2");

  invalidator->AddObserver(&handler1);
  invalidator->AddObserver(&handler2);

  // Registering both handlers for the same topic. First call should succeed,
  // second should fail.
  EXPECT_TRUE(invalidator->UpdateRegisteredTopics(&handler1, {kTopic1}));
  EXPECT_FALSE(invalidator->UpdateRegisteredTopics(&handler2, {kTopic1}));

  // |handler1| should still own subscription to the topic and deregistration
  // of its topics should update subscriptions.
  EXPECT_TRUE(invalidator->UpdateRegisteredTopics(&handler1, /*topics=*/{}));
  EXPECT_TRUE(invalidator->GetAllSubscribedTopics().empty());

  invalidator->RemoveObserver(&handler2);
  invalidator->RemoveObserver(&handler1);
}

// Make sure that passing an empty set to UpdateRegisteredTopics clears the
// corresponding entries for the handler.
TEST_F(InvalidatorRegistrarWithMemoryTest, EmptySetUnregisters) {
  TestingPrefServiceSimple pref_service;
  InvalidatorRegistrarWithMemory::RegisterProfilePrefs(pref_service.registry());

  auto invalidator = std::make_unique<InvalidatorRegistrarWithMemory>(
      &pref_service, "sender_id");

  FakeInvalidationHandler handler1("owner_1");

  // Control observer.
  FakeInvalidationHandler handler2("owner_2");

  invalidator->AddObserver(&handler1);
  invalidator->AddObserver(&handler2);

  EXPECT_TRUE(
      invalidator->UpdateRegisteredTopics(&handler1, {kTopic1, kTopic2}));
  EXPECT_TRUE(invalidator->UpdateRegisteredTopics(&handler2, {kTopic3}));

  // Unregister the topics for the first observer. It should not receive any
  // further invalidations.
  EXPECT_TRUE(invalidator->UpdateRegisteredTopics(&handler1, {}));

  invalidator->UpdateInvalidatorState(INVALIDATIONS_ENABLED);
  EXPECT_EQ(INVALIDATIONS_ENABLED, handler1.GetInvalidatorState());
  EXPECT_EQ(INVALIDATIONS_ENABLED, handler2.GetInvalidatorState());

  {
    Dispatch(*invalidator, kInv1, kInv2, kInv3);

    EXPECT_EQ(0, handler1.GetInvalidationCount());
    EXPECT_EQ(1, handler2.GetInvalidationCount());
  }

  invalidator->UpdateInvalidatorState(TRANSIENT_INVALIDATION_ERROR);
  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, handler1.GetInvalidatorState());
  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, handler2.GetInvalidatorState());

  invalidator->RemoveObserver(&handler2);
  invalidator->RemoveObserver(&handler1);
}

TEST_F(InvalidatorRegistrarWithMemoryTest, RestoresInterestingTopics) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kRestoreInterestingTopicsFeature);

  TestingPrefServiceSimple pref_service;
  InvalidatorRegistrarWithMemory::RegisterProfilePrefs(pref_service.registry());

  // Set up some previously-registered topics in the pref.
  constexpr char kStoredTopicsJson[] =
      R"({"sender_id": {
            "topic_1": {"handler": "handler_1", "is_public": true},
            "topic_2": {"handler": "handler_2", "is_public": true},
            "topic_3": "handler_3",
            "topic_4_1": {"handler": "handler_4", "is_public": false},
            "topic_4_2": {"handler": "handler_4", "is_public": false},
            "topic_4_3": {"handler": "handler_4", "is_public": false}
      }})";

  ASSERT_OK_AND_ASSIGN(
      auto stored_topics,
      base::JSONReader::ReadAndReturnValueWithError(kStoredTopicsJson));
  pref_service.Set(kTopicsToHandler, std::move(stored_topics));

  // Create an invalidator and make sure it correctly restored state from the
  // pref.
  auto invalidator = std::make_unique<InvalidatorRegistrarWithMemory>(
      &pref_service, "sender_id");

  std::map<std::string, TopicMetadata> expected_subscribed_topics{
      {"topic_1", TopicMetadata{true}},    {"topic_2", TopicMetadata{true}},
      {"topic_3", TopicMetadata{false}},   {"topic_4_1", TopicMetadata{false}},
      {"topic_4_2", TopicMetadata{false}}, {"topic_4_3", TopicMetadata{false}},
  };

  EXPECT_EQ(expected_subscribed_topics, invalidator->GetAllSubscribedTopics());
}

// This test verifies that topics are not unsubscribed after browser restart
// even if it's not registered using UpdateRegisteredTopics() method. This is
// useful for Sync invalidations to prevent unsubscribing from some data types
// which require more time to initialize and which are added later in following
// UpdateRegisteredTopics() calls.
//
// TODO(crbug.com/1051893): make the unsubscription behaviour consistent
// regardless of browser restart in between.
TEST_F(InvalidatorRegistrarWithMemoryTest,
       ShouldKeepSubscriptionsAfterRestart) {
  const TopicData kTopic1(/*name=*/"topic_1", /*is_public=*/true);
  const TopicData kTopic2(/*name=*/"topic_2", /*is_public=*/true);

  TestingPrefServiceSimple pref_service;
  InvalidatorRegistrarWithMemory::RegisterProfilePrefs(pref_service.registry());

  // Set up some previously-registered topics in the pref.
  constexpr char kStoredTopicsJson[] =
      R"({"sender_id": {
            "topic_1": {"handler": "handler", "is_public": true},
            "topic_2": {"handler": "handler", "is_public": true}
      }})";

  ASSERT_OK_AND_ASSIGN(
      auto stored_topics,
      base::JSONReader::ReadAndReturnValueWithError(kStoredTopicsJson));
  pref_service.Set(kTopicsToHandler, std::move(stored_topics));

  auto invalidator = std::make_unique<InvalidatorRegistrarWithMemory>(
      &pref_service, "sender_id");
  FakeInvalidationHandler handler("handler");
  invalidator->AddObserver(&handler);

  // Verify that all topics are successfully subscribed but not registered by
  // the |handler|.
  ASSERT_THAT(invalidator->GetRegisteredTopics(&handler), IsEmpty());
  ASSERT_THAT(invalidator->GetAllSubscribedTopics(),
              UnorderedElementsAre(
                  Pair(kTopic1.name, TopicMetadata{kTopic1.is_public}),
                  Pair(kTopic2.name, TopicMetadata{kTopic2.is_public})));

  // Register fo only one topic, but the previous subscriptions to other topics
  // should be kept.
  ASSERT_TRUE(invalidator->UpdateRegisteredTopics(&handler, {kTopic1}));
  EXPECT_THAT(invalidator->GetRegisteredTopics(&handler),
              UnorderedElementsAre(
                  Pair(kTopic1.name, TopicMetadata{kTopic1.is_public})));
  EXPECT_THAT(invalidator->GetAllSubscribedTopics(),
              UnorderedElementsAre(
                  Pair(kTopic1.name, TopicMetadata{kTopic1.is_public}),
                  Pair(kTopic2.name, TopicMetadata{kTopic2.is_public})));

  // To unsubscribe from the topics which were added before browser restart, the
  // handler needs to explicitly register this topic, then unregister again.
  ASSERT_TRUE(
      invalidator->UpdateRegisteredTopics(&handler, {kTopic1, kTopic2}));
  ASSERT_TRUE(invalidator->UpdateRegisteredTopics(&handler, {kTopic1}));
  EXPECT_THAT(invalidator->GetRegisteredTopics(&handler),
              UnorderedElementsAre(
                  Pair(kTopic1.name, TopicMetadata{kTopic1.is_public})));
  EXPECT_THAT(invalidator->GetAllSubscribedTopics(),
              UnorderedElementsAre(
                  Pair(kTopic1.name, TopicMetadata{kTopic1.is_public})));

  invalidator->RemoveObserver(&handler);
}

}  // namespace

}  // namespace invalidation
