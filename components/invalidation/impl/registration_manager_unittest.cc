// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/registration_manager.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include "base/macros.h"
#include "base/stl_util.h"
#include "base/test/task_environment.h"
#include "components/invalidation/public/invalidation_util.h"
#include "google/cacheinvalidation/include/invalidation-client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

// Fake registration manager that lets you override jitter.
class FakeRegistrationManager : public RegistrationManager {
 public:
  explicit FakeRegistrationManager(
      invalidation::InvalidationClient* invalidation_client)
      : RegistrationManager(invalidation_client),
        jitter_(0.0) {}

  ~FakeRegistrationManager() override {}

  void SetJitter(double jitter) {
    jitter_ = jitter;
  }

 protected:
  double GetJitter() override { return jitter_; }

 private:
  double jitter_;

  DISALLOW_COPY_AND_ASSIGN(FakeRegistrationManager);
};

// Fake invalidation client that just stores the currently-registered
// object IDs.
class FakeInvalidationClient : public invalidation::InvalidationClient {
 public:
  FakeInvalidationClient() {}

  ~FakeInvalidationClient() override {}

  void LoseRegistration(const invalidation::ObjectId& oid) {
    EXPECT_TRUE(base::Contains(registered_ids_, oid));
    registered_ids_.erase(oid);
  }

  void LoseAllRegistrations() {
    registered_ids_.clear();
  }

  // invalidation::InvalidationClient implementation.

  void Start() override {}
  void Stop() override {}
  void Acknowledge(const invalidation::AckHandle& handle) override {}

  void Register(const invalidation::ObjectId& oid) override {
    EXPECT_FALSE(base::Contains(registered_ids_, oid));
    registered_ids_.insert(oid);
  }

  void Register(const std::vector<invalidation::ObjectId>& oids) override {
    // Unused for now.
  }

  void Unregister(const invalidation::ObjectId& oid) override {
    EXPECT_TRUE(base::Contains(registered_ids_, oid));
    registered_ids_.erase(oid);
  }

  void Unregister(const std::vector<invalidation::ObjectId>& oids) override {
    // Unused for now.
  }

  const ObjectIdSet& GetRegisteredIdsForTest() const {
    return registered_ids_;
  }

 private:
  ObjectIdSet registered_ids_;

  DISALLOW_COPY_AND_ASSIGN(FakeInvalidationClient);
};

size_t kObjectIdsCount = 5;

invalidation::ObjectId GetIdForIndex(size_t index) {
  char name[2] = "a";
  name[0] += static_cast<char>(index);
  return invalidation::ObjectId(1 + index, name);
}

ObjectIdSet GetSequenceOfIdsStartingAt(size_t start, size_t count) {
  ObjectIdSet ids;
  for (size_t i = start; i < start + count; ++i)
    ids.insert(GetIdForIndex(i));
  return ids;
}

ObjectIdSet GetSequenceOfIds(size_t count) {
  return GetSequenceOfIdsStartingAt(0, count);
}

void ExpectPendingRegistrations(
    const ObjectIdSet& expected_pending_ids,
    double expected_delay_seconds,
    const RegistrationManager::PendingRegistrationMap& pending_registrations) {
  ObjectIdSet pending_ids;
  for (auto it = pending_registrations.begin();
       it != pending_registrations.end(); ++it) {
    SCOPED_TRACE(ObjectIdToString(it->first));
    pending_ids.insert(it->first);
    base::TimeDelta offset =
        it->second.last_registration_request -
        it->second.registration_attempt;
    base::TimeDelta expected_delay =
        base::TimeDelta::FromSeconds(
            static_cast<int64_t>(expected_delay_seconds)) +
        offset;
    // TODO(akalin): Add base::PrintTo() for base::Time and
    // base::TimeDeltas.
    EXPECT_EQ(expected_delay, it->second.delay)
        << expected_delay.InMicroseconds()
        << ", " << it->second.delay.InMicroseconds();
    if (it->second.delay <= base::TimeDelta()) {
      EXPECT_EQ(base::TimeDelta(), it->second.actual_delay);
    } else {
      EXPECT_EQ(it->second.actual_delay, it->second.delay);
    }
  }
  EXPECT_EQ(expected_pending_ids, pending_ids);
}

class RegistrationManagerTest : public testing::Test {
 protected:
  RegistrationManagerTest()
      : fake_registration_manager_(&fake_invalidation_client_) {}

  ~RegistrationManagerTest() override {}

  void LoseRegistrations(const ObjectIdSet& oids) {
    for (auto it = oids.begin(); it != oids.end(); ++it) {
      fake_invalidation_client_.LoseRegistration(*it);
      fake_registration_manager_.MarkRegistrationLost(*it);
    }
  }

  void DisableIds(const ObjectIdSet& oids) {
    for (auto it = oids.begin(); it != oids.end(); ++it) {
      fake_invalidation_client_.LoseRegistration(*it);
      fake_registration_manager_.DisableId(*it);
    }
  }

  // Used by MarkRegistrationLostBackoff* tests.
  void RunBackoffTest(double jitter) {
    fake_registration_manager_.SetJitter(jitter);
    ObjectIdSet ids = GetSequenceOfIds(kObjectIdsCount);
    fake_registration_manager_.UpdateRegisteredIds(ids);

    // Lose some ids.
    ObjectIdSet lost_ids = GetSequenceOfIds(2);
    LoseRegistrations(lost_ids);
    ExpectPendingRegistrations(
        lost_ids, 0.0,
        fake_registration_manager_.GetPendingRegistrationsForTest());

    // Trigger another failure to start delaying.
    fake_registration_manager_.FirePendingRegistrationsForTest();
    LoseRegistrations(lost_ids);

    double scaled_jitter =
        jitter * RegistrationManager::kRegistrationDelayMaxJitter;

    double expected_delay =
        RegistrationManager::kInitialRegistrationDelaySeconds *
        (1.0 + scaled_jitter);
    expected_delay = std::floor(expected_delay);
    ExpectPendingRegistrations(
        lost_ids, expected_delay,
        fake_registration_manager_.GetPendingRegistrationsForTest());

    // Trigger another failure.
    fake_registration_manager_.FirePendingRegistrationsForTest();
    LoseRegistrations(lost_ids);
    expected_delay *=
        RegistrationManager::kRegistrationDelayExponent + scaled_jitter;
    expected_delay = std::floor(expected_delay);
    ExpectPendingRegistrations(
        lost_ids, expected_delay,
        fake_registration_manager_.GetPendingRegistrationsForTest());

    // Trigger enough failures to hit the ceiling.
    while (expected_delay < RegistrationManager::kMaxRegistrationDelaySeconds) {
      fake_registration_manager_.FirePendingRegistrationsForTest();
      LoseRegistrations(lost_ids);
      expected_delay *=
          RegistrationManager::kRegistrationDelayExponent + scaled_jitter;
      expected_delay = std::floor(expected_delay);
    }
    ExpectPendingRegistrations(
        lost_ids,
        RegistrationManager::kMaxRegistrationDelaySeconds,
        fake_registration_manager_.GetPendingRegistrationsForTest());
  }

  FakeInvalidationClient fake_invalidation_client_;
  FakeRegistrationManager fake_registration_manager_;

 private:
  // Needed by timers in RegistrationManager.
  base::test::SingleThreadTaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(RegistrationManagerTest);
};

// Basic test of UpdateRegisteredIds to make sure we properly register
// new IDs and unregister any IDs no longer in the set.
TEST_F(RegistrationManagerTest, UpdateRegisteredIds) {
  ObjectIdSet ids = GetSequenceOfIds(kObjectIdsCount - 1);

  EXPECT_TRUE(fake_registration_manager_.GetRegisteredIdsForTest().empty());
  EXPECT_TRUE(fake_invalidation_client_.GetRegisteredIdsForTest().empty());

  ObjectIdSet expected_unregistered_ids;

  ObjectIdSet unregistered_ids =
      fake_registration_manager_.UpdateRegisteredIds(ids);
  EXPECT_EQ(expected_unregistered_ids, unregistered_ids);
  EXPECT_EQ(ids, fake_registration_manager_.GetRegisteredIdsForTest());
  EXPECT_EQ(ids, fake_invalidation_client_.GetRegisteredIdsForTest());

  ids.insert(GetIdForIndex(kObjectIdsCount - 1));
  ids.erase(GetIdForIndex(kObjectIdsCount - 2));
  unregistered_ids = fake_registration_manager_.UpdateRegisteredIds(ids);
  expected_unregistered_ids.insert(GetIdForIndex(kObjectIdsCount - 2));
  EXPECT_EQ(expected_unregistered_ids, unregistered_ids);
  EXPECT_EQ(ids, fake_registration_manager_.GetRegisteredIdsForTest());
  EXPECT_EQ(ids, fake_invalidation_client_.GetRegisteredIdsForTest());
}

int GetRoundedBackoff(double retry_interval, double jitter) {
  const double kInitialRetryInterval = 3.0;
  const double kMinRetryInterval = 2.0;
  const double kMaxRetryInterval = 20.0;
  const double kBackoffExponent = 2.0;
  const double kMaxJitter = 0.5;

  return static_cast<int>(
      RegistrationManager::CalculateBackoff(retry_interval,
                                            kInitialRetryInterval,
                                            kMinRetryInterval,
                                            kMaxRetryInterval,
                                            kBackoffExponent,
                                            jitter,
                                            kMaxJitter));
}

TEST_F(RegistrationManagerTest, CalculateBackoff) {
  // Test initial.
  EXPECT_EQ(2, GetRoundedBackoff(0.0, -1.0));
  EXPECT_EQ(3, GetRoundedBackoff(0.0,  0.0));
  EXPECT_EQ(4, GetRoundedBackoff(0.0, +1.0));

  // Test non-initial.
  EXPECT_EQ(4, GetRoundedBackoff(3.0, -1.0));
  EXPECT_EQ(6, GetRoundedBackoff(3.0,  0.0));
  EXPECT_EQ(7, GetRoundedBackoff(3.0, +1.0));

  EXPECT_EQ(7, GetRoundedBackoff(5.0, -1.0));
  EXPECT_EQ(10, GetRoundedBackoff(5.0,  0.0));
  EXPECT_EQ(12, GetRoundedBackoff(5.0, +1.0));

  // Test ceiling.
  EXPECT_EQ(19, GetRoundedBackoff(13.0, -1.0));
  EXPECT_EQ(20, GetRoundedBackoff(13.0,  0.0));
  EXPECT_EQ(20, GetRoundedBackoff(13.0, +1.0));
}

// Losing a registration should queue automatic re-registration.
TEST_F(RegistrationManagerTest, MarkRegistrationLost) {
  ObjectIdSet ids = GetSequenceOfIds(kObjectIdsCount);

  fake_registration_manager_.UpdateRegisteredIds(ids);
  EXPECT_TRUE(
      fake_registration_manager_.GetPendingRegistrationsForTest().empty());

  // Lose some ids.
  ObjectIdSet lost_ids = GetSequenceOfIds(3);
  ObjectIdSet non_lost_ids = GetSequenceOfIdsStartingAt(3, kObjectIdsCount - 3);
  LoseRegistrations(lost_ids);
  ExpectPendingRegistrations(
      lost_ids, 0.0,
      fake_registration_manager_.GetPendingRegistrationsForTest());
  EXPECT_EQ(non_lost_ids, fake_registration_manager_.GetRegisteredIdsForTest());
  EXPECT_EQ(non_lost_ids, fake_invalidation_client_.GetRegisteredIdsForTest());

  // Pretend we waited long enough to re-register.
  fake_registration_manager_.FirePendingRegistrationsForTest();
  EXPECT_EQ(ids, fake_registration_manager_.GetRegisteredIdsForTest());
  EXPECT_EQ(ids, fake_invalidation_client_.GetRegisteredIdsForTest());
}

TEST_F(RegistrationManagerTest, MarkRegistrationLostBackoffLow) {
  RunBackoffTest(-1.0);
}

TEST_F(RegistrationManagerTest, MarkRegistrationLostBackoffMid) {
  RunBackoffTest(0.0);
}

TEST_F(RegistrationManagerTest, MarkRegistrationLostBackoffHigh) {
  RunBackoffTest(+1.0);
}

// Exponential backoff on lost registrations should be reset to zero if
// UpdateRegisteredIds is called.
TEST_F(RegistrationManagerTest, MarkRegistrationLostBackoffReset) {
  ObjectIdSet ids = GetSequenceOfIds(kObjectIdsCount);

  fake_registration_manager_.UpdateRegisteredIds(ids);

  // Lose some ids.
  ObjectIdSet lost_ids = GetSequenceOfIds(2);
  LoseRegistrations(lost_ids);
  ExpectPendingRegistrations(
      lost_ids, 0.0,
      fake_registration_manager_.GetPendingRegistrationsForTest());

  // Trigger another failure to start delaying.
  fake_registration_manager_.FirePendingRegistrationsForTest();
  LoseRegistrations(lost_ids);
  double expected_delay =
      RegistrationManager::kInitialRegistrationDelaySeconds;
  ExpectPendingRegistrations(
      lost_ids, expected_delay,
      fake_registration_manager_.GetPendingRegistrationsForTest());

  // Set ids again.
  fake_registration_manager_.UpdateRegisteredIds(ids);
  ExpectPendingRegistrations(
      ObjectIdSet(),
      0.0,
      fake_registration_manager_.GetPendingRegistrationsForTest());
}

TEST_F(RegistrationManagerTest, MarkAllRegistrationsLost) {
  ObjectIdSet ids = GetSequenceOfIds(kObjectIdsCount);

  fake_registration_manager_.UpdateRegisteredIds(ids);

  fake_invalidation_client_.LoseAllRegistrations();
  fake_registration_manager_.MarkAllRegistrationsLost();

  EXPECT_TRUE(fake_registration_manager_.GetRegisteredIdsForTest().empty());
  EXPECT_TRUE(fake_invalidation_client_.GetRegisteredIdsForTest().empty());

  ExpectPendingRegistrations(
      ids, 0.0,
      fake_registration_manager_.GetPendingRegistrationsForTest());

  // Trigger another failure to start delaying.
  fake_registration_manager_.FirePendingRegistrationsForTest();
  fake_invalidation_client_.LoseAllRegistrations();
  fake_registration_manager_.MarkAllRegistrationsLost();
  double expected_delay =
      RegistrationManager::kInitialRegistrationDelaySeconds;
  ExpectPendingRegistrations(
      ids, expected_delay,
      fake_registration_manager_.GetPendingRegistrationsForTest());

  // Pretend we waited long enough to re-register.
  fake_registration_manager_.FirePendingRegistrationsForTest();
  EXPECT_EQ(ids, fake_registration_manager_.GetRegisteredIdsForTest());
  EXPECT_EQ(ids, fake_invalidation_client_.GetRegisteredIdsForTest());
}

// IDs that are disabled should not be re-registered by UpdateRegisteredIds or
// automatic re-registration if that registration is lost.
TEST_F(RegistrationManagerTest, DisableId) {
  ObjectIdSet ids = GetSequenceOfIds(kObjectIdsCount);

  fake_registration_manager_.UpdateRegisteredIds(ids);
  EXPECT_TRUE(
      fake_registration_manager_.GetPendingRegistrationsForTest().empty());

  // Disable some ids.
  ObjectIdSet disabled_ids = GetSequenceOfIds(3);
  ObjectIdSet enabled_ids = GetSequenceOfIdsStartingAt(3, kObjectIdsCount - 3);
  DisableIds(disabled_ids);
  ExpectPendingRegistrations(
      ObjectIdSet(),
      0.0,
      fake_registration_manager_.GetPendingRegistrationsForTest());
  EXPECT_EQ(enabled_ids, fake_registration_manager_.GetRegisteredIdsForTest());
  EXPECT_EQ(enabled_ids, fake_invalidation_client_.GetRegisteredIdsForTest());

  fake_registration_manager_.UpdateRegisteredIds(ids);
  EXPECT_EQ(enabled_ids, fake_registration_manager_.GetRegisteredIdsForTest());

  fake_registration_manager_.MarkRegistrationLost(
      *disabled_ids.begin());
  ExpectPendingRegistrations(
      ObjectIdSet(),
      0.0,
      fake_registration_manager_.GetPendingRegistrationsForTest());

  fake_registration_manager_.MarkAllRegistrationsLost();
  ExpectPendingRegistrations(
      enabled_ids, 0.0,
      fake_registration_manager_.GetPendingRegistrationsForTest());
}

}  // namespace
}  // namespace syncer
