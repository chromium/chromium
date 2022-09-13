// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/invalidation_logger.h"

#include "base/values.h"
#include "components/invalidation/impl/invalidation_logger_observer.h"
#include "components/invalidation/public/invalidation.h"
#include "components/invalidation/public/topic_invalidation_map.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace invalidation {

class InvalidationLoggerObserverTest : public InvalidationLoggerObserver {
 public:
  InvalidationLoggerObserverTest() { ResetStates(); }

  void ResetStates() {
    registration_change_received = false;
    state_received = false;
    update_id_received = false;
    debug_message_received = false;
    invalidation_received = false;
    detailed_status_received = false;
    updated_topics_replicated = std::map<std::string, TopicCountMap>();
    registered_handlers = std::set<std::string>();
  }

  void OnRegistrationChange(const std::set<std::string>& handlers) override {
    registered_handlers = handlers;
    registration_change_received = true;
  }

  void OnStateChange(const InvalidatorState& new_state,
                     const base::Time& last_change_timestamp) override {
    state_received = true;
  }

  void OnUpdatedTopics(const std::string& handler,
                       const TopicCountMap& topics_counts) override {
    update_id_received = true;
    updated_topics_replicated[handler] = topics_counts;
  }

  void OnDebugMessage(const base::Value::Dict& details) override {
    debug_message_received = true;
  }

  void OnInvalidation(const TopicInvalidationMap& new_invalidations) override {
    invalidation_received = true;
  }

  void OnDetailedStatus(base::Value::Dict details) override {
    detailed_status_received = true;
  }

  bool registration_change_received;
  bool state_received;
  bool update_id_received;
  bool debug_message_received;
  bool invalidation_received;
  bool detailed_status_received;
  std::map<std::string, TopicCountMap> updated_topics_replicated;
  std::set<std::string> registered_handlers;
};

// Test that the callbacks are actually being called when observers are
// registered and don't produce any other callback in the meantime.
TEST(InvalidationLoggerTest, TestCallbacks) {
  InvalidationLogger log;
  InvalidationLoggerObserverTest observer_test;

  log.RegisterObserver(&observer_test);
  log.OnStateChange(INVALIDATIONS_ENABLED);
  EXPECT_TRUE(observer_test.state_received);
  EXPECT_FALSE(observer_test.update_id_received);
  EXPECT_FALSE(observer_test.registration_change_received);
  EXPECT_FALSE(observer_test.invalidation_received);
  EXPECT_FALSE(observer_test.debug_message_received);
  EXPECT_FALSE(observer_test.detailed_status_received);

  observer_test.ResetStates();

  log.OnInvalidation(TopicInvalidationMap());
  EXPECT_TRUE(observer_test.invalidation_received);
  EXPECT_FALSE(observer_test.state_received);
  EXPECT_FALSE(observer_test.update_id_received);
  EXPECT_FALSE(observer_test.registration_change_received);
  EXPECT_FALSE(observer_test.debug_message_received);
  EXPECT_FALSE(observer_test.detailed_status_received);

  log.UnregisterObserver(&observer_test);
}

// Test that after registering an observer and then unregistering it
// no callbacks regarding that observer are called.
// (i.e. the observer is cleanly removed)
TEST(InvalidationLoggerTest, TestReleaseOfObserver) {
  InvalidationLogger log;
  InvalidationLoggerObserverTest observer_test;

  log.RegisterObserver(&observer_test);
  log.UnregisterObserver(&observer_test);

  log.OnInvalidation(TopicInvalidationMap());
  log.OnStateChange(INVALIDATIONS_ENABLED);
  log.OnRegistration(std::string());
  log.OnUnregistration(std::string());
  log.OnDebugMessage(base::Value::Dict());
  log.OnUpdatedTopics(std::map<std::string, Topics>());
  EXPECT_FALSE(observer_test.registration_change_received);
  EXPECT_FALSE(observer_test.update_id_received);
  EXPECT_FALSE(observer_test.invalidation_received);
  EXPECT_FALSE(observer_test.state_received);
  EXPECT_FALSE(observer_test.debug_message_received);
  EXPECT_FALSE(observer_test.detailed_status_received);
}

// Test the EmitContet in InvalidationLogger is actually
// sending state and updateIds notifications.
TEST(InvalidationLoggerTest, TestEmitContent) {
  InvalidationLogger log;
  InvalidationLoggerObserverTest observer_test;

  log.RegisterObserver(&observer_test);
  EXPECT_FALSE(observer_test.state_received);
  EXPECT_FALSE(observer_test.update_id_received);
  log.EmitContent();
  // Expect state and registered handlers only because no Ids were registered.
  EXPECT_TRUE(observer_test.state_received);
  EXPECT_TRUE(observer_test.registration_change_received);
  EXPECT_FALSE(observer_test.update_id_received);
  EXPECT_FALSE(observer_test.invalidation_received);
  EXPECT_FALSE(observer_test.debug_message_received);
  EXPECT_FALSE(observer_test.detailed_status_received);

  observer_test.ResetStates();
  std::map<std::string, Topics> test_map;
  test_map["Test"] = Topics();
  log.OnUpdatedTopics(test_map);
  EXPECT_TRUE(observer_test.update_id_received);
  observer_test.ResetStates();

  log.EmitContent();
  // Expect now state, ids and registered handlers change.
  EXPECT_TRUE(observer_test.state_received);
  EXPECT_TRUE(observer_test.update_id_received);
  EXPECT_TRUE(observer_test.registration_change_received);
  EXPECT_FALSE(observer_test.invalidation_received);
  EXPECT_FALSE(observer_test.debug_message_received);
  EXPECT_FALSE(observer_test.detailed_status_received);
  log.UnregisterObserver(&observer_test);
}

// Test that the OnUpdatedTopics() notification actually sends the same Topic
// that was sent to the Observer.
// The ObserverTest rebuilds the map that was sent in pieces by the logger.
TEST(InvalidationLoggerTest, TestUpdatedTopicsMap) {
  InvalidationLogger log;
  InvalidationLoggerObserverTest observer_test;
  std::map<std::string, Topics> send_test_map;
  std::map<std::string, TopicCountMap> expected_received_map;
  log.RegisterObserver(&observer_test);

  Topics topics_a;
  TopicCountMap topics_counts_a;

  Topic t1 = "Topic1";
  topics_a.emplace(t1, TopicMetadata{/*is_public=*/false});
  topics_counts_a[t1] = 0;

  Topic t2 = "Topic2";
  topics_a.emplace(t2, TopicMetadata{/*is_public=*/false});
  topics_counts_a[t2] = 0;

  Topics topics_b;
  TopicCountMap topics_counts_b;

  Topic t3 = "Topic3";
  topics_b.emplace(t3, TopicMetadata{/*is_public=*/false});
  topics_counts_b[t3] = 0;

  send_test_map["TestA"] = topics_a;
  send_test_map["TestB"] = topics_b;
  expected_received_map["TestA"] = topics_counts_a;
  expected_received_map["TestB"] = topics_counts_b;

  // Send the topics registered for the two different handler name.
  log.OnUpdatedTopics(send_test_map);
  EXPECT_EQ(expected_received_map, observer_test.updated_topics_replicated);

  Topics topics_b2;
  TopicCountMap topics_counts_b2;

  Topic t4 = "Topic4";
  topics_b2.emplace(t4, TopicMetadata{/*is_public=*/false});
  topics_counts_b2[t4] = 0;

  Topic t5 = "Topic5";
  topics_b2.emplace(t5, TopicMetadata{/*is_public=*/false});
  topics_counts_b2[t5] = 0;

  send_test_map["TestB"] = topics_b2;
  expected_received_map["TestB"] = topics_counts_b2;

  // Test now that if we replace the registered topics for TestB, the
  // original don't show up again.
  log.OnUpdatedTopics(send_test_map);
  EXPECT_EQ(expected_received_map, observer_test.updated_topics_replicated);

  // The emit content should return the same map too.
  observer_test.ResetStates();
  log.EmitContent();
  EXPECT_EQ(expected_received_map, observer_test.updated_topics_replicated);
  log.UnregisterObserver(&observer_test);
}

// Test that the invalidation notification changes the total count
// of invalidations received for that datatype.
TEST(InvalidationLoggerTest, TestInvalidtionsTotalCount) {
  InvalidationLogger log;
  InvalidationLoggerObserverTest observer_test;
  log.RegisterObserver(&observer_test);

  std::map<std::string, Topics> send_test_map;
  std::map<std::string, TopicCountMap> expected_received_map;
  Topics topics;
  TopicCountMap topics_counts;

  Topic t1 = "Topic1";
  topics.emplace(t1, TopicMetadata{/*is_public=*/false});
  topics_counts[t1] = 1;

  // Generate invalidation for |t1| only.
  TopicInvalidationMap fake_invalidations;
  fake_invalidations.Insert(Invalidation::InitUnknownVersion(t1));

  Topic t2 = "Topic2";
  topics.emplace(t2, TopicMetadata{/*is_public=*/false});
  topics_counts[t2] = 0;

  // Register the two Topics and send an invalidation only for |t1|.
  send_test_map["Test"] = topics;
  log.OnUpdatedTopics(send_test_map);
  log.OnInvalidation(fake_invalidations);

  expected_received_map["Test"] = topics_counts;

  // Reset the state of the observer to receive the Topics with the count of
  // invalidations received (1 and 0).
  observer_test.ResetStates();
  log.EmitContent();
  EXPECT_EQ(expected_received_map, observer_test.updated_topics_replicated);

  log.UnregisterObserver(&observer_test);
}

// Test that registered handlers are being sent to the observers.
TEST(InvalidationLoggerTest, TestRegisteredHandlers) {
  InvalidationLogger log;
  InvalidationLoggerObserverTest observer_test;
  log.RegisterObserver(&observer_test);

  log.OnRegistration(std::string("FakeHandler1"));
  std::set<std::string> test_set;
  test_set.insert("FakeHandler1");
  EXPECT_TRUE(observer_test.registration_change_received);
  EXPECT_EQ(observer_test.registered_handlers, test_set);

  observer_test.ResetStates();
  log.OnRegistration(std::string("FakeHandler2"));
  test_set.insert("FakeHandler2");
  EXPECT_TRUE(observer_test.registration_change_received);
  EXPECT_EQ(observer_test.registered_handlers, test_set);

  observer_test.ResetStates();
  log.OnUnregistration(std::string("FakeHandler2"));
  test_set.erase("FakeHandler2");
  EXPECT_TRUE(observer_test.registration_change_received);
  EXPECT_EQ(observer_test.registered_handlers, test_set);

  log.UnregisterObserver(&observer_test);
}

}  // namespace invalidation
