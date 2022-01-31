// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/invalidator_registrar_with_memory.h"

#include <memory>

#include "base/feature_list.h"
#include "base/json/json_reader.h"
#include "base/test/scoped_feature_list.h"
#include "components/invalidation/impl/fake_invalidation_handler.h"
#include "components/invalidation/public/invalidation.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/invalidation/public/invalidator_state.h"
#include "components/invalidation/public/topic_invalidation_map.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace invalidation {

namespace {

// Initialize the invalidator, register a handler, register some topics for that
// handler, and then unregister the handler, dispatching invalidations in
// between. The handler should only see invalidations when it's registered and
// its topics are registered.
TEST(InvalidatorRegistrarWithMemoryTest, Basic) {
  const TopicData kTopic1(/*name=*/"a", /*is_public=*/false);
  const TopicData kTopic2(/*name=*/"b", /*is_public=*/false);
  const TopicData kTopic3(/*name=*/"c", /*is_public=*/false);

  TestingPrefServiceSimple pref_service;
  InvalidatorRegistrarWithMemory::RegisterProfilePrefs(pref_service.registry());

  auto invalidator = std::make_unique<InvalidatorRegistrarWithMemory>(
      &pref_service, "sender_id", /*migrate_old_prefs=*/false);

  FakeInvalidationHandler handler;
  invalidator->RegisterHandler(&handler);

  TopicInvalidationMap invalidation_map;
  invalidation_map.Insert(Invalidation::Init(kTopic1.name, 1, "1"));
  invalidation_map.Insert(Invalidation::Init(kTopic2.name, 2, "2"));
  invalidation_map.Insert(Invalidation::Init(kTopic3.name, 3, "3"));

  // Should be ignored since no topics are registered to |handler|.
  invalidator->DispatchInvalidationsToHandlers(invalidation_map);
  EXPECT_EQ(0, handler.GetInvalidationCount());

  EXPECT_TRUE(
      invalidator->UpdateRegisteredTopics(&handler, {kTopic1, kTopic2}));

  invalidator->UpdateInvalidatorState(INVALIDATIONS_ENABLED);
  EXPECT_EQ(INVALIDATIONS_ENABLED, handler.GetInvalidatorState());

  TopicInvalidationMap expected_invalidations;
  expected_invalidations.Insert(Invalidation::Init(kTopic1.name, 1, "1"));
  expected_invalidations.Insert(Invalidation::Init(kTopic2.name, 2, "2"));

  invalidator->DispatchInvalidationsToHandlers(invalidation_map);
  EXPECT_EQ(1, handler.GetInvalidationCount());
  EXPECT_EQ(expected_invalidations, handler.GetLastInvalidationMap());

  // Remove kTopic1, add kTopic3.
  EXPECT_TRUE(
      invalidator->UpdateRegisteredTopics(&handler, {kTopic2, kTopic3}));

  expected_invalidations = TopicInvalidationMap();
  expected_invalidations.Insert(Invalidation::Init(kTopic2.name, 2, "2"));
  expected_invalidations.Insert(Invalidation::Init(kTopic3.name, 3, "3"));

  // Removed topic should not be notified, newly-added ones should.
  invalidator->DispatchInvalidationsToHandlers(invalidation_map);
  EXPECT_EQ(2, handler.GetInvalidationCount());
  EXPECT_EQ(expected_invalidations, handler.GetLastInvalidationMap());

  invalidator->UpdateInvalidatorState(TRANSIENT_INVALIDATION_ERROR);
  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, handler.GetInvalidatorState());

  invalidator->UpdateInvalidatorState(INVALIDATION_CREDENTIALS_REJECTED);
  EXPECT_EQ(INVALIDATION_CREDENTIALS_REJECTED, handler.GetInvalidatorState());

  invalidator->UnregisterHandler(&handler);

  // Should be ignored since |handler| isn't registered anymore.
  invalidator->DispatchInvalidationsToHandlers(invalidation_map);
  EXPECT_EQ(2, handler.GetInvalidationCount());
}

// Register handlers and some topics for those handlers, register a handler with
// no topics, and register a handler with some topics but unregister it. Then,
// dispatch some invalidations. Handlers that are not registered should not get
// invalidations, and the ones that have registered topics should receive
// invalidations for those topics.
TEST(InvalidatorRegistrarWithMemoryTest, MultipleHandlers) {
  const TopicData kTopic1(/*name=*/"a", /*is_public=*/false);
  const TopicData kTopic2(/*name=*/"b", /*is_public=*/false);
  const TopicData kTopic3(/*name=*/"c", /*is_public=*/false);
  const TopicData kTopic4(/*name=*/"d", /*is_public=*/false);

  TestingPrefServiceSimple pref_service;
  InvalidatorRegistrarWithMemory::RegisterProfilePrefs(pref_service.registry());

  auto invalidator = std::make_unique<InvalidatorRegistrarWithMemory>(
      &pref_service, "sender_id", /*migrate_old_prefs=*/false);

  FakeInvalidationHandler handler1;
  FakeInvalidationHandler handler2;
  FakeInvalidationHandler handler3;
  FakeInvalidationHandler handler4;

  invalidator->RegisterHandler(&handler1);
  invalidator->RegisterHandler(&handler2);
  invalidator->RegisterHandler(&handler3);
  invalidator->RegisterHandler(&handler4);

  EXPECT_TRUE(
      invalidator->UpdateRegisteredTopics(&handler1, {kTopic1, kTopic2}));
  EXPECT_TRUE(invalidator->UpdateRegisteredTopics(&handler2, {kTopic3}));
  // Don't register any IDs for handler3.
  EXPECT_TRUE(invalidator->UpdateRegisteredTopics(&handler4, {kTopic4}));

  invalidator->UnregisterHandler(&handler4);

  invalidator->UpdateInvalidatorState(INVALIDATIONS_ENABLED);
  EXPECT_EQ(INVALIDATIONS_ENABLED, handler1.GetInvalidatorState());
  EXPECT_EQ(INVALIDATIONS_ENABLED, handler2.GetInvalidatorState());
  EXPECT_EQ(INVALIDATIONS_ENABLED, handler3.GetInvalidatorState());
  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, handler4.GetInvalidatorState());

  TopicInvalidationMap invalidation_map;
  invalidation_map.Insert(Invalidation::Init(kTopic1.name, 1, "1"));
  invalidation_map.Insert(Invalidation::Init(kTopic2.name, 2, "2"));
  invalidation_map.Insert(Invalidation::Init(kTopic3.name, 3, "3"));
  invalidation_map.Insert(Invalidation::Init(kTopic4.name, 4, "4"));

  invalidator->DispatchInvalidationsToHandlers(invalidation_map);

  TopicInvalidationMap expected_invalidations1;
  expected_invalidations1.Insert(Invalidation::Init(kTopic1.name, 1, "1"));
  expected_invalidations1.Insert(Invalidation::Init(kTopic2.name, 2, "2"));

  EXPECT_EQ(1, handler1.GetInvalidationCount());
  EXPECT_EQ(expected_invalidations1, handler1.GetLastInvalidationMap());

  TopicInvalidationMap expected_invalidations2;
  expected_invalidations2.Insert(Invalidation::Init(kTopic3.name, 3, "3"));

  EXPECT_EQ(1, handler2.GetInvalidationCount());
  EXPECT_EQ(expected_invalidations2, handler2.GetLastInvalidationMap());

  EXPECT_EQ(0, handler3.GetInvalidationCount());
  EXPECT_EQ(0, handler4.GetInvalidationCount());

  invalidator->UpdateInvalidatorState(TRANSIENT_INVALIDATION_ERROR);
  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, handler1.GetInvalidatorState());
  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, handler2.GetInvalidatorState());
  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, handler3.GetInvalidatorState());
  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, handler4.GetInvalidatorState());

  invalidator->UnregisterHandler(&handler3);
  invalidator->UnregisterHandler(&handler2);
  invalidator->UnregisterHandler(&handler1);
}

// Multiple registrations by different handlers on the same topic should
// return false.
TEST(InvalidatorRegistrarWithMemoryTest, MultipleRegistrations) {
  const TopicData kTopic1(/*name=*/"a", /*is_public=*/false);

  TestingPrefServiceSimple pref_service;
  InvalidatorRegistrarWithMemory::RegisterProfilePrefs(pref_service.registry());

  auto invalidator = std::make_unique<InvalidatorRegistrarWithMemory>(
      &pref_service, "sender_id", /*migrate_old_prefs=*/false);

  FakeInvalidationHandler handler1("owner1");
  FakeInvalidationHandler handler2("owner2");

  invalidator->RegisterHandler(&handler1);
  invalidator->RegisterHandler(&handler2);

  // Registering both handlers for the same topic. First call should succeed,
  // second should fail.
  EXPECT_TRUE(invalidator->UpdateRegisteredTopics(&handler1, {kTopic1}));
  EXPECT_FALSE(invalidator->UpdateRegisteredTopics(&handler2, {kTopic1}));

  // |handler1| should still own subscription to the topic and deregistration
  // of its topics should update subscriptions.
  EXPECT_TRUE(invalidator->UpdateRegisteredTopics(&handler1, /*topics=*/{}));
  EXPECT_TRUE(invalidator->GetAllSubscribedTopics().empty());

  invalidator->UnregisterHandler(&handler2);
  invalidator->UnregisterHandler(&handler1);
}

// Make sure that passing an empty set to UpdateRegisteredTopics clears the
// corresponding entries for the handler.
TEST(InvalidatorRegistrarWithMemoryTest, EmptySetUnregisters) {
  const TopicData kTopic1(/*name=*/"a", /*is_public=*/false);
  const TopicData kTopic2(/*name=*/"b", /*is_public=*/false);
  const TopicData kTopic3(/*name=*/"c", /*is_public=*/false);

  TestingPrefServiceSimple pref_service;
  InvalidatorRegistrarWithMemory::RegisterProfilePrefs(pref_service.registry());

  auto invalidator = std::make_unique<InvalidatorRegistrarWithMemory>(
      &pref_service, "sender_id", /*migrate_old_prefs=*/false);

  FakeInvalidationHandler handler1;

  // Control observer.
  FakeInvalidationHandler handler2;

  invalidator->RegisterHandler(&handler1);
  invalidator->RegisterHandler(&handler2);

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
    TopicInvalidationMap invalidation_map;
    invalidation_map.Insert(Invalidation::Init(kTopic1.name, 1, "1"));
    invalidation_map.Insert(Invalidation::Init(kTopic2.name, 2, "2"));
    invalidation_map.Insert(Invalidation::Init(kTopic3.name, 3, "3"));
    invalidator->DispatchInvalidationsToHandlers(invalidation_map);
    EXPECT_EQ(0, handler1.GetInvalidationCount());
    EXPECT_EQ(1, handler2.GetInvalidationCount());
  }

  invalidator->UpdateInvalidatorState(TRANSIENT_INVALIDATION_ERROR);
  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, handler1.GetInvalidatorState());
  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, handler2.GetInvalidatorState());

  invalidator->UnregisterHandler(&handler2);
  invalidator->UnregisterHandler(&handler1);
}

TEST(InvalidatorRegistrarWithMemoryTest, RestoresInterestingTopics) {
  constexpr char kTopicsToHandler[] =
      "invalidation.per_sender_topics_to_handler";

  const base::Feature restore_interesting_topics_feature{
      "InvalidatorRestoreInterestingTopics", base::FEATURE_ENABLED_BY_DEFAULT};
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(restore_interesting_topics_feature);

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

  auto stored_topics =
      base::JSONReader::ReadAndReturnValueWithError(kStoredTopicsJson);
  ASSERT_TRUE(stored_topics.value) << stored_topics.error_message;
  pref_service.Set(kTopicsToHandler, std::move(*stored_topics.value));

  // Create an invalidator and make sure it correctly restored state from the
  // pref.
  auto invalidator = std::make_unique<InvalidatorRegistrarWithMemory>(
      &pref_service, "sender_id", /*migrate_old_prefs=*/false);

  std::map<std::string, TopicMetadata> expected_subscribed_topics{
      {"topic_1", TopicMetadata{true}},    {"topic_2", TopicMetadata{true}},
      {"topic_3", TopicMetadata{false}},   {"topic_4_1", TopicMetadata{false}},
      {"topic_4_2", TopicMetadata{false}}, {"topic_4_3", TopicMetadata{false}},
  };

  EXPECT_EQ(expected_subscribed_topics, invalidator->GetAllSubscribedTopics());
}

TEST(InvalidatorRegistrarWithMemoryTest,
     ClearsTopicsWithObsoleteOwnerNamesWhenPrefIsEmpty) {
  constexpr char kTopicsToHandler[] =
      "invalidation.per_sender_topics_to_handler";

  TestingPrefServiceSimple pref_service;
  InvalidatorRegistrarWithMemory::RegisterProfilePrefs(pref_service.registry());
  ASSERT_EQ(base::Value(base::Value::Type::DICTIONARY),
            *pref_service.Get(kTopicsToHandler));

  InvalidatorRegistrarWithMemory::ClearTopicsWithObsoleteOwnerNames(
      &pref_service);

  EXPECT_EQ(base::Value(base::Value::Type::DICTIONARY),
            *pref_service.Get(kTopicsToHandler));
}

TEST(InvalidatorRegistrarWithMemoryTest, ClearsTopicsWithObsoleteOwnerNames) {
  constexpr char kTopicsToHandler[] =
      "invalidation.per_sender_topics_to_handler";

  TestingPrefServiceSimple pref_service;
  InvalidatorRegistrarWithMemory::RegisterProfilePrefs(pref_service.registry());

  // Set up some previously-registered topics in the pref.
  constexpr char kInitialStoredTopics[] =
      R"({"sender_without_cloud": {
            "topic_1": {"handler": "NonCloud_1", "is_public": true},
            "topic_2": {"handler": "NonCloud_2", "is_public": true},
            "topic_3": {"handler": "NonCloud_3", "is_public": true}
          },
          "sender_with_cloud": {
            "topic_4": {"handler": "NonCloud_4", "is_public": true},
            "topic_5": {"handler": "Cloud", "is_public": true},
            "topic_6": "NonCloud_5",
            "topic_7": "RemoteCommand"
          },
          "sender_full_of_cloud": {
            "topic_8": {"handler": "Cloud", "is_public": true},
            "topic_9": {"handler": "RemoteCommand", "is_public": true},
            "topic_10": {"handler": "Cloud", "is_public": true}
        }})";
  auto initial_stored_topics =
      base::JSONReader::ReadAndReturnValueWithError(kInitialStoredTopics);
  ASSERT_TRUE(initial_stored_topics.value)
      << initial_stored_topics.error_message;

  pref_service.Set(kTopicsToHandler, initial_stored_topics.value->Clone());

  ASSERT_EQ(*initial_stored_topics.value, *pref_service.Get(kTopicsToHandler));

  InvalidatorRegistrarWithMemory::ClearTopicsWithObsoleteOwnerNames(
      &pref_service);

  // Topics 5, 7, 8, 9 and 10 are expected to be gone.
  constexpr char kExpectedStoredTopics[] =
      R"({"sender_without_cloud": {
            "topic_1": {"handler": "NonCloud_1", "is_public": true},
            "topic_2": {"handler": "NonCloud_2", "is_public": true},
            "topic_3": {"handler": "NonCloud_3", "is_public": true}
          },
          "sender_with_cloud": {
            "topic_4": {"handler": "NonCloud_4", "is_public": true},
            "topic_6": "NonCloud_5"
          },
          "sender_full_of_cloud": {}})";
  auto expected_stored_topics =
      base::JSONReader::ReadAndReturnValueWithError(kExpectedStoredTopics);
  ASSERT_TRUE(expected_stored_topics.value)
      << expected_stored_topics.error_message;

  EXPECT_EQ(*expected_stored_topics.value, *pref_service.Get(kTopicsToHandler));
}

}  // namespace

}  // namespace invalidation
