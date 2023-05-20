// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class defines tests that implementations of InvalidationService should
// pass in order to be conformant.  Here's how you use it to test your
// implementation.
//
// Say your class is called MyInvalidationService.  Then you need to define a
// class called MyInvalidationServiceTestDelegate in
// my_invalidation_frontend_unittest.cc like this:
//
//   class MyInvalidationServiceTestDelegate {
//    public:
//     MyInvalidationServiceTestDelegate() ...
//
//     ~MyInvalidationServiceTestDelegate() {
//       // DestroyInvalidator() may not be explicitly called by tests.
//       DestroyInvalidator();
//     }
//
//     // Create the InvalidationService implementation with the given params.
//     void CreateInvalidationService() {
//       ...
//     }
//
//     // Should return the InvalidationService implementation.  Only called
//     // after CreateInvalidator and before DestroyInvalidator.
//     MyInvalidationService* GetInvalidationService() {
//       ...
//     }
//
//     // The Trigger* functions below should block until the effects of
//     // the call are visible on the current thread.
//
//     // Should cause OnInvalidatorStateChange() to be called on all
//     // observers of the InvalidationService implementation with the given
//     // parameters.
//     void TriggerOnInvalidatorStateChange(InvalidatorState state) {
//       ...
//     }
//
//     // Should cause OnIncomingInvalidation() to be called on all
//     // observers of the InvalidationService implementation with the given
//     // parameters.
//     void TriggerOnIncomingInvalidation(
//         const TopicInvalidationMap& invalidation_map) {
//       ...
//     }
//   };
//
// The InvalidationServiceTest test harness will have a member variable of
// this delegate type and will call its functions in the various
// tests.
//
// Then you simply #include this file as well as gtest.h and add the
// following statement to my_sync_notifier_unittest.cc:
//
//   INSTANTIATE_TYPED_TEST_SUITE_P(
//       MyInvalidationService,
//       InvalidationServiceTest,
//       MyInvalidatorTestDelegate);
//
// Easy!

#ifndef COMPONENTS_INVALIDATION_IMPL_INVALIDATION_SERVICE_TEST_TEMPLATE_H_
#define COMPONENTS_INVALIDATION_IMPL_INVALIDATION_SERVICE_TEST_TEMPLATE_H_

#include "base/compiler_specific.h"
#include "base/memory/raw_ref.h"
#include "components/invalidation/impl/fake_invalidation_handler.h"
#include "components/invalidation/impl/topic_invalidation_map_test_util.h"
#include "components/invalidation/public/ack_handle.h"
#include "components/invalidation/public/invalidation.h"
#include "components/invalidation/public/invalidation_service.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/invalidation/public/topic_invalidation_map.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace invalidation {

template <typename InvalidatorTestDelegate>
class InvalidationServiceTest : public testing::Test {
 protected:
  InvalidationServiceTest() = default;

  InvalidationService* CreateAndInitializeInvalidationService() {
    this->delegate_.CreateInvalidationService();
    return this->delegate_.GetInvalidationService();
  }

  InvalidatorTestDelegate delegate_;

  const Topic topic1 = "BOOKMARK";
  const Topic topic2 = "PREFERENCE";
  const Topic topic3 = "AUTOFILL";
  const Topic topic4 = "PUSH_MESSAGE";
};

TYPED_TEST_SUITE_P(InvalidationServiceTest);

// Initialize the invalidator, register a handler, register some IDs for that
// handler, and then unregister the handler, dispatching invalidations in
// between.  The handler should only see invalidations when its registered and
// its IDs are registered.
TYPED_TEST_P(InvalidationServiceTest, Basic) {
  InvalidationService* const invalidator =
      this->CreateAndInitializeInvalidationService();

  FakeInvalidationHandler handler("owner");

  invalidator->RegisterInvalidationHandler(&handler);

  TopicInvalidationMap invalidation_map;
  invalidation_map.Insert(Invalidation::Init(this->topic1, 1, "1"));
  invalidation_map.Insert(Invalidation::Init(this->topic2, 2, "2"));
  invalidation_map.Insert(Invalidation::Init(this->topic3, 3, "3"));

  // Should be ignored since no IDs are registered to |handler|.
  this->delegate_.TriggerOnIncomingInvalidation(invalidation_map);
  EXPECT_EQ(0, handler.GetInvalidationCount());

  TopicSet topics;
  topics.insert(this->topic1);
  topics.insert(this->topic2);
  EXPECT_TRUE(invalidator->UpdateInterestedTopics(&handler, topics));

  this->delegate_.TriggerOnInvalidatorStateChange(INVALIDATIONS_ENABLED);
  EXPECT_EQ(INVALIDATIONS_ENABLED, handler.GetInvalidatorState());

  TopicInvalidationMap expected_invalidations;
  expected_invalidations.Insert(Invalidation::Init(this->topic1, 1, "1"));
  expected_invalidations.Insert(Invalidation::Init(this->topic2, 2, "2"));

  this->delegate_.TriggerOnIncomingInvalidation(invalidation_map);
  EXPECT_EQ(1, handler.GetInvalidationCount());
  EXPECT_THAT(expected_invalidations, Eq(handler.GetLastInvalidationMap()));

  topics.erase(this->topic1);
  topics.insert(this->topic3);
  EXPECT_TRUE(invalidator->UpdateInterestedTopics(&handler, topics));

  expected_invalidations = TopicInvalidationMap();
  expected_invalidations.Insert(Invalidation::Init(this->topic2, 2, "2"));
  expected_invalidations.Insert(Invalidation::Init(this->topic3, 3, "3"));

  // Removed Topics should not be notified, newly-added ones should.
  this->delegate_.TriggerOnIncomingInvalidation(invalidation_map);
  EXPECT_EQ(2, handler.GetInvalidationCount());
  EXPECT_THAT(expected_invalidations, Eq(handler.GetLastInvalidationMap()));

  this->delegate_.TriggerOnInvalidatorStateChange(TRANSIENT_INVALIDATION_ERROR);
  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, handler.GetInvalidatorState());

  this->delegate_.TriggerOnInvalidatorStateChange(INVALIDATIONS_ENABLED);
  EXPECT_EQ(INVALIDATIONS_ENABLED, handler.GetInvalidatorState());

  invalidator->UnregisterInvalidationHandler(&handler);

  // Should be ignored since |handler| isn't registered anymore.
  this->delegate_.TriggerOnIncomingInvalidation(invalidation_map);
  EXPECT_EQ(2, handler.GetInvalidationCount());
}

// Register handlers and some topics for those handlers, register a handler
// with no topics, and register a handler with some topics but unregister it.
// Then, dispatch some invalidations and invalidations.  Handlers that are
// registered should get invalidations, and the ones that have registered
// topics should receive invalidations for those topics.
TYPED_TEST_P(InvalidationServiceTest, MultipleHandlers) {
  InvalidationService* const invalidator =
      this->CreateAndInitializeInvalidationService();

  FakeInvalidationHandler handler1(/*owner=*/"owner_1");
  FakeInvalidationHandler handler2(/*owner=*/"owner_2");
  FakeInvalidationHandler handler3(/*owner=*/"owner_3");
  FakeInvalidationHandler handler4(/*owner=*/"owner_4");

  invalidator->RegisterInvalidationHandler(&handler1);
  invalidator->RegisterInvalidationHandler(&handler2);
  invalidator->RegisterInvalidationHandler(&handler3);
  invalidator->RegisterInvalidationHandler(&handler4);

  {
    TopicSet topics;
    topics.insert(this->topic1);
    topics.insert(this->topic2);
    EXPECT_TRUE(invalidator->UpdateInterestedTopics(&handler1, topics));
  }

  {
    TopicSet topics;
    topics.insert(this->topic3);
    EXPECT_TRUE(invalidator->UpdateInterestedTopics(&handler2, topics));
  }

  // Don't register any topics for handler3.

  {
    TopicSet topics;
    topics.insert(this->topic4);
    EXPECT_TRUE(invalidator->UpdateInterestedTopics(&handler4, topics));
  }

  invalidator->UnregisterInvalidationHandler(&handler4);

  this->delegate_.TriggerOnInvalidatorStateChange(INVALIDATIONS_ENABLED);
  EXPECT_EQ(INVALIDATIONS_ENABLED, handler1.GetInvalidatorState());
  EXPECT_EQ(INVALIDATIONS_ENABLED, handler2.GetInvalidatorState());
  EXPECT_EQ(INVALIDATIONS_ENABLED, handler3.GetInvalidatorState());
  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, handler4.GetInvalidatorState());

  {
    TopicInvalidationMap invalidation_map;
    invalidation_map.Insert(Invalidation::Init(this->topic1, 1, "1"));
    invalidation_map.Insert(Invalidation::Init(this->topic2, 2, "2"));
    invalidation_map.Insert(Invalidation::Init(this->topic3, 3, "3"));
    invalidation_map.Insert(Invalidation::Init(this->topic4, 4, "4"));
    this->delegate_.TriggerOnIncomingInvalidation(invalidation_map);

    TopicInvalidationMap expected_invalidations;
    expected_invalidations.Insert(Invalidation::Init(this->topic1, 1, "1"));
    expected_invalidations.Insert(Invalidation::Init(this->topic2, 2, "2"));

    EXPECT_EQ(1, handler1.GetInvalidationCount());
    EXPECT_THAT(expected_invalidations, Eq(handler1.GetLastInvalidationMap()));

    expected_invalidations = TopicInvalidationMap();
    expected_invalidations.Insert(Invalidation::Init(this->topic3, 3, "3"));

    EXPECT_EQ(1, handler2.GetInvalidationCount());
    EXPECT_THAT(expected_invalidations, Eq(handler2.GetLastInvalidationMap()));

    EXPECT_EQ(0, handler3.GetInvalidationCount());
    EXPECT_EQ(0, handler4.GetInvalidationCount());
  }

  this->delegate_.TriggerOnInvalidatorStateChange(TRANSIENT_INVALIDATION_ERROR);
  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, handler1.GetInvalidatorState());
  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, handler2.GetInvalidatorState());
  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, handler3.GetInvalidatorState());
  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, handler4.GetInvalidatorState());

  invalidator->UnregisterInvalidationHandler(&handler3);
  invalidator->UnregisterInvalidationHandler(&handler2);
  invalidator->UnregisterInvalidationHandler(&handler1);
}

// Multiple registrations by different handlers on the same Topic should return
// false.
TYPED_TEST_P(InvalidationServiceTest, MultipleRegistrations) {
  InvalidationService* const invalidator =
      this->CreateAndInitializeInvalidationService();

  FakeInvalidationHandler handler1(/*owner=*/"owner_1");
  FakeInvalidationHandler handler2(/*owner=*/"owner_2");

  invalidator->RegisterInvalidationHandler(&handler1);
  invalidator->RegisterInvalidationHandler(&handler2);

  // Registering both handlers for the same topic. First call should succeed,
  // second should fail.
  TopicSet topics;
  topics.insert(this->topic1);
  EXPECT_TRUE(invalidator->UpdateInterestedTopics(&handler1, topics));
  EXPECT_FALSE(invalidator->UpdateInterestedTopics(&handler2, topics));

  invalidator->UnregisterInvalidationHandler(&handler2);
  invalidator->UnregisterInvalidationHandler(&handler1);
}

// Make sure that passing an empty set to UpdateInterestedTopics clears
// the corresponding entries for the handler.
TYPED_TEST_P(InvalidationServiceTest, EmptySetUnregisters) {
  InvalidationService* const invalidator =
      this->CreateAndInitializeInvalidationService();

  FakeInvalidationHandler handler1(/*owner=*/"owner_1");

  // Control observer.
  FakeInvalidationHandler handler2(/*owner=*/"owner_2");

  invalidator->RegisterInvalidationHandler(&handler1);
  invalidator->RegisterInvalidationHandler(&handler2);

  {
    TopicSet topics;
    topics.insert(this->topic1);
    topics.insert(this->topic2);
    EXPECT_TRUE(invalidator->UpdateInterestedTopics(&handler1, topics));
  }

  {
    TopicSet topics;
    topics.insert(this->topic3);
    EXPECT_TRUE(invalidator->UpdateInterestedTopics(&handler2, topics));
  }

  // Unregister the topics for the first observer. It should not receive any
  // further invalidations.
  EXPECT_TRUE(invalidator->UpdateInterestedTopics(&handler1, TopicSet()));

  this->delegate_.TriggerOnInvalidatorStateChange(INVALIDATIONS_ENABLED);
  EXPECT_EQ(INVALIDATIONS_ENABLED, handler1.GetInvalidatorState());
  EXPECT_EQ(INVALIDATIONS_ENABLED, handler2.GetInvalidatorState());

  {
    TopicInvalidationMap invalidation_map;
    invalidation_map.Insert(Invalidation::Init(this->topic1, 1, "1"));
    invalidation_map.Insert(Invalidation::Init(this->topic2, 2, "2"));
    invalidation_map.Insert(Invalidation::Init(this->topic3, 3, "3"));
    this->delegate_.TriggerOnIncomingInvalidation(invalidation_map);
    EXPECT_EQ(0, handler1.GetInvalidationCount());
    EXPECT_EQ(1, handler2.GetInvalidationCount());
  }

  this->delegate_.TriggerOnInvalidatorStateChange(TRANSIENT_INVALIDATION_ERROR);
  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, handler1.GetInvalidatorState());
  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, handler2.GetInvalidatorState());

  invalidator->UnregisterInvalidationHandler(&handler2);
  invalidator->UnregisterInvalidationHandler(&handler1);
}

// A FakeInvalidationHandler that is "bound" to a specific
// InvalidationService.  This is for cross-referencing state information with
// the bound InvalidationService.
class BoundFakeInvalidationHandler : public FakeInvalidationHandler {
 public:
  BoundFakeInvalidationHandler(const InvalidationService& invalidator,
                               const std::string& owner);

  BoundFakeInvalidationHandler(const BoundFakeInvalidationHandler&) = delete;
  BoundFakeInvalidationHandler& operator=(const BoundFakeInvalidationHandler&) =
      delete;

  ~BoundFakeInvalidationHandler() override;

  // Returns the last return value of GetInvalidatorState() on the
  // bound invalidator from the last time the invalidator state
  // changed.
  InvalidatorState GetLastRetrievedState() const;

  // InvalidationHandler implementation.
  void OnInvalidatorStateChange(InvalidatorState state) override;

 private:
  const raw_ref<const InvalidationService> invalidator_;
  InvalidatorState last_retrieved_state_;
};

TYPED_TEST_P(InvalidationServiceTest, GetInvalidatorStateAlwaysCurrent) {
  InvalidationService* const invalidator =
      this->CreateAndInitializeInvalidationService();

  BoundFakeInvalidationHandler handler(*invalidator, "owner");
  invalidator->RegisterInvalidationHandler(&handler);

  this->delegate_.TriggerOnInvalidatorStateChange(INVALIDATIONS_ENABLED);
  EXPECT_EQ(INVALIDATIONS_ENABLED, handler.GetInvalidatorState());
  EXPECT_EQ(INVALIDATIONS_ENABLED, handler.GetLastRetrievedState());

  this->delegate_.TriggerOnInvalidatorStateChange(TRANSIENT_INVALIDATION_ERROR);
  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, handler.GetInvalidatorState());
  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, handler.GetLastRetrievedState());

  invalidator->UnregisterInvalidationHandler(&handler);
}

REGISTER_TYPED_TEST_SUITE_P(InvalidationServiceTest,
                            Basic,
                            MultipleHandlers,
                            MultipleRegistrations,
                            EmptySetUnregisters,
                            GetInvalidatorStateAlwaysCurrent);

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_IMPL_INVALIDATION_SERVICE_TEST_TEMPLATE_H_
