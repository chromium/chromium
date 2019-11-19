// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class defines tests that implementations of Invalidator should pass in
// order to be conformant.  Here's how you use it to test your implementation.
//
// Say your class is called MyInvalidator.  Then you need to define a class
// called MyInvalidatorTestDelegate in my_sync_notifier_unittest.cc like this:
//
//   class MyInvalidatorTestDelegate {
//    public:
//     MyInvalidatorTestDelegate() ...
//
//     ~MyInvalidatorTestDelegate() {
//       // DestroyInvalidator() may not be explicitly called by tests.
//       DestroyInvalidator();
//     }
//
//     // Create the Invalidator implementation with the given parameters.
//     void CreateInvalidator(
//         const std::string& initial_state,
//         const base::WeakPtr<InvalidationStateTracker>&
//             invalidation_state_tracker) {
//       ...
//     }
//
//     // Should return the Invalidator implementation.  Only called after
//     // CreateInvalidator and before DestroyInvalidator.
//     MyInvalidator* GetInvalidator() {
//       ...
//     }
//
//     // Destroy the Invalidator implementation.
//     void DestroyInvalidator() {
//       ...
//     }
//
//     // Called after a call to SetUniqueId(), or UpdateCredentials() on the
//     // Invalidator implementation.  Should block until the effects of the
//     // call are visible on the current thread.
//     void WaitForInvalidator() {
//       ...
//     }
//
//     // The Trigger* functions below should block until the effects of
//     // the call are visible on the current thread.
//
//     // Should cause OnInvalidatorStateChange() to be called on all
//     // observers of the Invalidator implementation with the given
//     // parameters.
//     void TriggerOnInvalidatorStateChange(InvalidatorState state) {
//       ...
//     }
//
//     // Should cause OnIncomingInvalidation() to be called on all
//     // observers of the Invalidator implementation with the given
//     // parameters.
//     void TriggerOnIncomingInvalidation(
//         const ObjectIdInvalidationMap& invalidation_map) {
//       ...
//     }
//   };
//
// The InvalidatorTest test harness will have a member variable of
// this delegate type and will call its functions in the various
// tests.
//
// Then you simply #include this file as well as gtest.h and add the
// following statement to my_sync_notifier_unittest.cc:
//
//   INSTANTIATE_TYPED_TEST_SUITE_P(
//       MyInvalidator, InvalidatorTest, MyInvalidatorTestDelegate);
//
// Easy!

#ifndef COMPONENTS_INVALIDATION_IMPL_INVALIDATOR_TEST_TEMPLATE_H_
#define COMPONENTS_INVALIDATION_IMPL_INVALIDATOR_TEST_TEMPLATE_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/invalidation/impl/fake_invalidation_handler.h"
#include "components/invalidation/impl/fake_invalidation_state_tracker.h"
#include "components/invalidation/impl/invalidator.h"
#include "components/invalidation/impl/object_id_invalidation_map_test_util.h"
#include "google/cacheinvalidation/include/types.h"
#include "google/cacheinvalidation/types.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

template <typename InvalidatorTestDelegate>
class InvalidatorTest : public testing::Test {
 protected:
  InvalidatorTest()
      : id1(ipc::invalidation::ObjectSource::TEST, "a"),
        id2(ipc::invalidation::ObjectSource::TEST, "b"),
        id3(ipc::invalidation::ObjectSource::TEST, "c"),
        id4(ipc::invalidation::ObjectSource::TEST, "d") {
  }

  Invalidator* CreateAndInitializeInvalidator() {
    this->delegate_.CreateInvalidator("fake_invalidator_client_id",
                                      "fake_initial_state",
                                      this->fake_tracker_.AsWeakPtr());
    Invalidator* const invalidator = this->delegate_.GetInvalidator();

    this->delegate_.WaitForInvalidator();
    invalidator->UpdateCredentials(CoreAccountId("foo@bar.com"), "fake_token");
    this->delegate_.WaitForInvalidator();

    return invalidator;
  }

  FakeInvalidationStateTracker fake_tracker_;
  InvalidatorTestDelegate delegate_;

  const invalidation::ObjectId id1;
  const invalidation::ObjectId id2;
  const invalidation::ObjectId id3;
  const invalidation::ObjectId id4;
};

TYPED_TEST_SUITE_P(InvalidatorTest);

// Initialize the invalidator, register a handler, register some IDs for that
// handler, and then unregister the handler, dispatching invalidations in
// between.  The handler should only see invalidations when its registered and
// its IDs are registered.
TYPED_TEST_P(InvalidatorTest, Basic) {
  Invalidator* const invalidator = this->CreateAndInitializeInvalidator();

  FakeInvalidationHandler handler;

  invalidator->RegisterHandler(&handler);

  ObjectIdInvalidationMap invalidation_map;
  invalidation_map.Insert(Invalidation::Init(this->id1, 1, "1"));
  invalidation_map.Insert(Invalidation::Init(this->id2, 2, "2"));
  invalidation_map.Insert(Invalidation::Init(this->id3, 3, "3"));

  // Should be ignored since no IDs are registered to |handler|.
  this->delegate_.TriggerOnIncomingInvalidation(invalidation_map);
  EXPECT_EQ(0, handler.GetInvalidationCount());

  ObjectIdSet ids;
  ids.insert(this->id1);
  ids.insert(this->id2);
  EXPECT_TRUE(invalidator->UpdateRegisteredIds(&handler, ids));

  this->delegate_.TriggerOnInvalidatorStateChange(INVALIDATIONS_ENABLED);
  EXPECT_EQ(INVALIDATIONS_ENABLED, handler.GetInvalidatorState());

  ObjectIdInvalidationMap expected_invalidations;
  expected_invalidations.Insert(Invalidation::Init(this->id1, 1, "1"));
  expected_invalidations.Insert(Invalidation::Init(this->id2, 2, "2"));

  this->delegate_.TriggerOnIncomingInvalidation(invalidation_map);
  EXPECT_EQ(1, handler.GetInvalidationCount());
  EXPECT_THAT(expected_invalidations, Eq(handler.GetLastInvalidationMap()));

  ids.erase(this->id1);
  ids.insert(this->id3);
  EXPECT_TRUE(invalidator->UpdateRegisteredIds(&handler, ids));

  expected_invalidations = ObjectIdInvalidationMap();
  expected_invalidations.Insert(Invalidation::Init(this->id2, 2, "2"));
  expected_invalidations.Insert(Invalidation::Init(this->id3, 3, "3"));

  // Removed object IDs should not be notified, newly-added ones should.
  this->delegate_.TriggerOnIncomingInvalidation(invalidation_map);
  EXPECT_EQ(2, handler.GetInvalidationCount());
  EXPECT_THAT(expected_invalidations, Eq(handler.GetLastInvalidationMap()));

  this->delegate_.TriggerOnInvalidatorStateChange(TRANSIENT_INVALIDATION_ERROR);
  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR,
            handler.GetInvalidatorState());

  this->delegate_.TriggerOnInvalidatorStateChange(
      INVALIDATION_CREDENTIALS_REJECTED);
  EXPECT_EQ(INVALIDATION_CREDENTIALS_REJECTED,
            handler.GetInvalidatorState());

  invalidator->UnregisterHandler(&handler);

  // Should be ignored since |handler| isn't registered anymore.
  this->delegate_.TriggerOnIncomingInvalidation(invalidation_map);
  EXPECT_EQ(2, handler.GetInvalidationCount());
}

// Register handlers and some IDs for those handlers, register a handler with
// no IDs, and register a handler with some IDs but unregister it.  Then,
// dispatch some invalidations and invalidations.  Handlers that are registered
// should get invalidations, and the ones that have registered IDs should
// receive invalidations for those IDs.
TYPED_TEST_P(InvalidatorTest, MultipleHandlers) {
  Invalidator* const invalidator = this->CreateAndInitializeInvalidator();

  FakeInvalidationHandler handler1;
  FakeInvalidationHandler handler2;
  FakeInvalidationHandler handler3;
  FakeInvalidationHandler handler4;

  invalidator->RegisterHandler(&handler1);
  invalidator->RegisterHandler(&handler2);
  invalidator->RegisterHandler(&handler3);
  invalidator->RegisterHandler(&handler4);

  {
    ObjectIdSet ids;
    ids.insert(this->id1);
    ids.insert(this->id2);
    EXPECT_TRUE(invalidator->UpdateRegisteredIds(&handler1, ids));
  }

  {
    ObjectIdSet ids;
    ids.insert(this->id3);
    EXPECT_TRUE(invalidator->UpdateRegisteredIds(&handler2, ids));
  }

  // Don't register any IDs for handler3.

  {
    ObjectIdSet ids;
    ids.insert(this->id4);
    EXPECT_TRUE(invalidator->UpdateRegisteredIds(&handler4, ids));
  }

  invalidator->UnregisterHandler(&handler4);

  this->delegate_.TriggerOnInvalidatorStateChange(INVALIDATIONS_ENABLED);
  EXPECT_EQ(INVALIDATIONS_ENABLED, handler1.GetInvalidatorState());
  EXPECT_EQ(INVALIDATIONS_ENABLED, handler2.GetInvalidatorState());
  EXPECT_EQ(INVALIDATIONS_ENABLED, handler3.GetInvalidatorState());
  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, handler4.GetInvalidatorState());

  {
    ObjectIdInvalidationMap invalidation_map;
    invalidation_map.Insert(Invalidation::Init(this->id1, 1, "1"));
    invalidation_map.Insert(Invalidation::Init(this->id2, 2, "2"));
    invalidation_map.Insert(Invalidation::Init(this->id3, 3, "3"));
    invalidation_map.Insert(Invalidation::Init(this->id4, 4, "4"));

    this->delegate_.TriggerOnIncomingInvalidation(invalidation_map);

    ObjectIdInvalidationMap expected_invalidations;
    expected_invalidations.Insert(Invalidation::Init(this->id1, 1, "1"));
    expected_invalidations.Insert(Invalidation::Init(this->id2, 2, "2"));

    EXPECT_EQ(1, handler1.GetInvalidationCount());
    EXPECT_THAT(expected_invalidations, Eq(handler1.GetLastInvalidationMap()));

    expected_invalidations = ObjectIdInvalidationMap();
    expected_invalidations.Insert(Invalidation::Init(this->id3, 3, "3"));

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

  invalidator->UnregisterHandler(&handler3);
  invalidator->UnregisterHandler(&handler2);
  invalidator->UnregisterHandler(&handler1);
}

// Multiple registrations by different handlers on the same object ID should
// return false.
TYPED_TEST_P(InvalidatorTest, MultipleRegistrations) {
  Invalidator* const invalidator = this->CreateAndInitializeInvalidator();

  FakeInvalidationHandler handler1;
  FakeInvalidationHandler handler2;

  invalidator->RegisterHandler(&handler1);
  invalidator->RegisterHandler(&handler2);

  // Registering both handlers for the same ObjectId. First call should succeed,
  // second should fail.
  ObjectIdSet ids;
  ids.insert(this->id1);
  EXPECT_TRUE(invalidator->UpdateRegisteredIds(&handler1, ids));
  EXPECT_FALSE(invalidator->UpdateRegisteredIds(&handler2, ids));

  invalidator->UnregisterHandler(&handler2);
  invalidator->UnregisterHandler(&handler1);
}

// Make sure that passing an empty set to UpdateRegisteredIds clears the
// corresponding entries for the handler.
TYPED_TEST_P(InvalidatorTest, EmptySetUnregisters) {
  Invalidator* const invalidator = this->CreateAndInitializeInvalidator();

  FakeInvalidationHandler handler1;

  // Control observer.
  FakeInvalidationHandler handler2;

  invalidator->RegisterHandler(&handler1);
  invalidator->RegisterHandler(&handler2);

  {
    ObjectIdSet ids;
    ids.insert(this->id1);
    ids.insert(this->id2);
    EXPECT_TRUE(invalidator->UpdateRegisteredIds(&handler1, ids));
  }

  {
    ObjectIdSet ids;
    ids.insert(this->id3);
    EXPECT_TRUE(invalidator->UpdateRegisteredIds(&handler2, ids));
  }

  // Unregister the IDs for the first observer. It should not receive any
  // further invalidations.
  EXPECT_TRUE(invalidator->UpdateRegisteredIds(&handler1, ObjectIdSet()));

  this->delegate_.TriggerOnInvalidatorStateChange(INVALIDATIONS_ENABLED);
  EXPECT_EQ(INVALIDATIONS_ENABLED, handler1.GetInvalidatorState());
  EXPECT_EQ(INVALIDATIONS_ENABLED, handler2.GetInvalidatorState());

  {
    ObjectIdInvalidationMap invalidation_map;
    invalidation_map.Insert(Invalidation::Init(this->id1, 1, "1"));
    invalidation_map.Insert(Invalidation::Init(this->id2, 2, "2"));
    invalidation_map.Insert(Invalidation::Init(this->id3, 3, "3"));
    this->delegate_.TriggerOnIncomingInvalidation(invalidation_map);
    EXPECT_EQ(0, handler1.GetInvalidationCount());
    EXPECT_EQ(1, handler2.GetInvalidationCount());
  }

  this->delegate_.TriggerOnInvalidatorStateChange(TRANSIENT_INVALIDATION_ERROR);
  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, handler1.GetInvalidatorState());
  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, handler2.GetInvalidatorState());

  invalidator->UnregisterHandler(&handler2);
  invalidator->UnregisterHandler(&handler1);
}

namespace internal {

// A FakeInvalidationHandler that is "bound" to a specific
// Invalidator.  This is for cross-referencing state information with
// the bound Invalidator.
class BoundFakeInvalidationHandler : public FakeInvalidationHandler {
 public:
  explicit BoundFakeInvalidationHandler(const Invalidator& invalidator);
  ~BoundFakeInvalidationHandler() override;

  // Returns the last return value of GetInvalidatorState() on the
  // bound invalidator from the last time the invalidator state
  // changed.
  InvalidatorState GetLastRetrievedState() const;

  // InvalidationHandler implementation.
  void OnInvalidatorStateChange(InvalidatorState state) override;

 private:
  const Invalidator& invalidator_;
  InvalidatorState last_retrieved_state_;

  DISALLOW_COPY_AND_ASSIGN(BoundFakeInvalidationHandler);
};

}  // namespace internal

TYPED_TEST_P(InvalidatorTest, GetInvalidatorStateAlwaysCurrent) {
  Invalidator* const invalidator = this->CreateAndInitializeInvalidator();

  internal::BoundFakeInvalidationHandler handler(*invalidator);
  invalidator->RegisterHandler(&handler);

  this->delegate_.TriggerOnInvalidatorStateChange(INVALIDATIONS_ENABLED);
  EXPECT_EQ(INVALIDATIONS_ENABLED, handler.GetInvalidatorState());
  EXPECT_EQ(INVALIDATIONS_ENABLED, handler.GetLastRetrievedState());

  this->delegate_.TriggerOnInvalidatorStateChange(TRANSIENT_INVALIDATION_ERROR);
  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, handler.GetInvalidatorState());
  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, handler.GetLastRetrievedState());

  invalidator->UnregisterHandler(&handler);
}

REGISTER_TYPED_TEST_SUITE_P(InvalidatorTest,
                            Basic,
                            MultipleHandlers,
                            MultipleRegistrations,
                            EmptySetUnregisters,
                            GetInvalidatorStateAlwaysCurrent);

}  // namespace syncer

#endif  // COMPONENTS_INVALIDATION_IMPL_INVALIDATOR_TEST_TEMPLATE_H_
