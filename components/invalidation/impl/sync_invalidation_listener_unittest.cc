// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <cstddef>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/invalidation/impl/fake_invalidation_state_tracker.h"
#include "components/invalidation/impl/push_client_channel.h"
#include "components/invalidation/impl/sync_invalidation_listener.h"
#include "components/invalidation/impl/unacked_invalidation_set_test_util.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/invalidation/public/object_id_invalidation_map.h"
#include "google/cacheinvalidation/include/invalidation-client.h"
#include "google/cacheinvalidation/include/types.h"
#include "jingle/notifier/listener/fake_push_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using invalidation::AckHandle;
using invalidation::ObjectId;

const char kClientId[] = "client_id";
const char kClientInfo[] = "client_info";

const char kState[] = "state";
const char kNewState[] = "new_state";

const char kPayload1[] = "payload1";
const char kPayload2[] = "payload2";

const int64_t kVersion1 = 1LL;
const int64_t kVersion2 = 2LL;

const int kChromeSyncSourceId = 1004;

struct AckHandleLessThan {
  bool operator()(const AckHandle& lhs, const AckHandle& rhs) const {
    return lhs.handle_data() < rhs.handle_data();
  }
};

typedef std::set<AckHandle, AckHandleLessThan> AckHandleSet;

// Fake invalidation::InvalidationClient implementation that keeps
// track of registered IDs and acked handles.
class FakeInvalidationClient : public invalidation::InvalidationClient {
 public:
  FakeInvalidationClient() : started_(false) {}
  ~FakeInvalidationClient() override {}

  const ObjectIdSet& GetRegisteredIds() const {
    return registered_ids_;
  }

  void ClearAckedHandles() {
    acked_handles_.clear();
  }

  bool IsAckedHandle(const AckHandle& ack_handle) const {
    return (acked_handles_.find(ack_handle) != acked_handles_.end());
  }

  // invalidation::InvalidationClient implementation.

  void Start() override { started_ = true; }

  void Stop() override { started_ = false; }

  void Register(const ObjectId& object_id) override {
    if (!started_) {
      ADD_FAILURE();
      return;
    }
    registered_ids_.insert(object_id);
  }

  void Register(const invalidation::vector<ObjectId>& object_ids) override {
    if (!started_) {
      ADD_FAILURE();
      return;
    }
    registered_ids_.insert(object_ids.begin(), object_ids.end());
  }

  void Unregister(const ObjectId& object_id) override {
    if (!started_) {
      ADD_FAILURE();
      return;
    }
    registered_ids_.erase(object_id);
  }

  void Unregister(const invalidation::vector<ObjectId>& object_ids) override {
    if (!started_) {
      ADD_FAILURE();
      return;
    }
    for (auto it = object_ids.begin(); it != object_ids.end(); ++it) {
      registered_ids_.erase(*it);
    }
  }

  void Acknowledge(const AckHandle& ack_handle) override {
    if (!started_) {
      ADD_FAILURE();
      return;
    }
    acked_handles_.insert(ack_handle);
  }

 private:
  bool started_;
  ObjectIdSet registered_ids_;
  AckHandleSet acked_handles_;
};

// Fake delegate tkat keeps track of invalidation counts, payloads,
// and state.
class FakeDelegate : public SyncInvalidationListener::Delegate {
 public:
  explicit FakeDelegate(SyncInvalidationListener* listener)
      : state_(TRANSIENT_INVALIDATION_ERROR) {}
  ~FakeDelegate() override {}

  size_t GetInvalidationCount(const ObjectId& id) const {
    auto it = invalidations_.find(id);
    if (it == invalidations_.end()) {
      return 0;
    } else {
      return it->second.size();
    }
  }

  int64_t GetVersion(const ObjectId& id) const {
    auto it = invalidations_.find(id);
    if (it == invalidations_.end()) {
      ADD_FAILURE() << "No invalidations for ID " << ObjectIdToString(id);
      return 0;
    } else {
      return it->second.back().version();
    }
  }

  std::string GetPayload(const ObjectId& id) const {
    auto it = invalidations_.find(id);
    if (it == invalidations_.end()) {
      ADD_FAILURE() << "No invalidations for ID " << ObjectIdToString(id);
      return nullptr;
    } else {
      return it->second.back().payload();
    }
  }

  bool IsUnknownVersion(const ObjectId& id) const {
    auto it = invalidations_.find(id);
    if (it == invalidations_.end()) {
      ADD_FAILURE() << "No invalidations for ID " << ObjectIdToString(id);
      return false;
    } else {
      return it->second.back().is_unknown_version();
    }
  }

  bool StartsWithUnknownVersion(const ObjectId& id) const {
    auto it = invalidations_.find(id);
    if (it == invalidations_.end()) {
      ADD_FAILURE() << "No invalidations for ID " << ObjectIdToString(id);
      return false;
    } else {
      return it->second.front().is_unknown_version();
    }
  }

  InvalidatorState GetInvalidatorState() const {
    return state_;
  }

  void AcknowledgeNthInvalidation(const ObjectId& id, size_t n) {
    List& list = invalidations_[id];
    auto it = list.begin() + n;
    it->Acknowledge();
  }

  void AcknowledgeAll(const ObjectId& id) {
    List& list = invalidations_[id];
    for (auto it = list.begin(); it != list.end(); ++it) {
      it->Acknowledge();
    }
  }

  void DropNthInvalidation(const ObjectId& id, size_t n) {
    List& list = invalidations_[id];
    auto it = list.begin() + n;
    it->Drop();
    dropped_invalidations_map_.erase(id);
    dropped_invalidations_map_.insert(std::make_pair(id, *it));
  }

  void RecoverFromDropEvent(const ObjectId& id) {
    auto it = dropped_invalidations_map_.find(id);
    if (it != dropped_invalidations_map_.end()) {
      it->second.Acknowledge();
      dropped_invalidations_map_.erase(it);
    }
  }

  // SyncInvalidationListener::Delegate implementation.
  void OnInvalidate(const ObjectIdInvalidationMap& invalidation_map) override {
    ObjectIdSet ids = invalidation_map.GetObjectIds();
    for (auto it = ids.begin(); it != ids.end(); ++it) {
      const SingleObjectInvalidationSet& incoming =
          invalidation_map.ForObject(*it);
      List& list = invalidations_[*it];
      list.insert(list.end(), incoming.begin(), incoming.end());
    }
  }

  void OnInvalidatorStateChange(InvalidatorState state) override {
    state_ = state;
  }

 private:
  typedef std::vector<Invalidation> List;
  typedef std::map<ObjectId, List, ObjectIdLessThan> Map;
  typedef std::map<ObjectId, Invalidation, ObjectIdLessThan> DropMap;

  Map invalidations_;
  InvalidatorState state_;
  DropMap dropped_invalidations_map_;
};

invalidation::InvalidationClient* CreateFakeInvalidationClient(
    FakeInvalidationClient** fake_invalidation_client,
    invalidation::SystemResources* resources,
    int client_type,
    const invalidation::string& client_name,
    const invalidation::string& application_name,
    invalidation::InvalidationListener* listener) {
  *fake_invalidation_client = new FakeInvalidationClient();
  return *fake_invalidation_client;
}

class SyncInvalidationListenerTest : public testing::Test {
 protected:
  SyncInvalidationListenerTest()
      : kBookmarksId_(kChromeSyncSourceId, "BOOKMARK"),
        kPreferencesId_(kChromeSyncSourceId, "PREFERENCE"),
        kExtensionsId_(kChromeSyncSourceId, "EXTENSION"),
        kAppsId_(kChromeSyncSourceId, "APP"),
        fake_push_client_(new notifier::FakePushClient()),
        fake_invalidation_client_(nullptr),
        listener_(base::WrapUnique(
            new PushClientChannel(base::WrapUnique(fake_push_client_)))),
        fake_delegate_(&listener_) {}

  void SetUp() override {
    StartClient();

    registered_ids_.insert(kBookmarksId_);
    registered_ids_.insert(kPreferencesId_);
    listener_.UpdateRegisteredIds(registered_ids_);
  }

  void TearDown() override { StopClient(); }

  // Restart client without re-registering IDs.
  void RestartClient() {
    StopClient();
    StartClient();
  }

  void StartClient() {
    fake_invalidation_client_ = nullptr;
    listener_.Start(
        base::Bind(&CreateFakeInvalidationClient, &fake_invalidation_client_),
        kClientId, kClientInfo, kState, fake_tracker_.GetSavedInvalidations(),
        fake_tracker_.AsWeakPtr(), base::ThreadTaskRunnerHandle::Get(),
        &fake_delegate_);
    DCHECK(fake_invalidation_client_);
  }

  void StopClient() {
    // listener_.StopForTest() stops the invalidation scheduler, which
    // deletes any pending tasks without running them.  Some tasks
    // "run and delete" another task, so they must be run in order to
    // avoid leaking the inner task.  listener_.StopForTest() does not
    // schedule any tasks, so it's both necessary and sufficient to
    // drain the task queue before calling it.
    FlushPendingWrites();
    fake_invalidation_client_ = nullptr;
    listener_.StopForTest();
  }

  size_t GetInvalidationCount(const ObjectId& id) const {
    return fake_delegate_.GetInvalidationCount(id);
  }

  int64_t GetVersion(const ObjectId& id) const {
    return fake_delegate_.GetVersion(id);
  }

  std::string GetPayload(const ObjectId& id) const {
    return fake_delegate_.GetPayload(id);
  }

  bool IsUnknownVersion(const ObjectId& id) const {
    return fake_delegate_.IsUnknownVersion(id);
  }

  bool StartsWithUnknownVersion(const ObjectId& id) const {
    return fake_delegate_.StartsWithUnknownVersion(id);
  }

  void AcknowledgeNthInvalidation(const ObjectId& id, size_t n) {
    fake_delegate_.AcknowledgeNthInvalidation(id, n);
  }

  void DropNthInvalidation(const ObjectId& id, size_t n) {
    return fake_delegate_.DropNthInvalidation(id, n);
  }

  void RecoverFromDropEvent(const ObjectId& id) {
    return fake_delegate_.RecoverFromDropEvent(id);
  }

  void AcknowledgeAll(const ObjectId& id) {
    fake_delegate_.AcknowledgeAll(id);
  }

  InvalidatorState GetInvalidatorState() const {
    return fake_delegate_.GetInvalidatorState();
  }

  std::string GetInvalidatorClientId() const {
    return fake_tracker_.GetInvalidatorClientId();
  }

  std::string GetBootstrapData() const {
    return fake_tracker_.GetBootstrapData();
  }

  UnackedInvalidationsMap GetSavedInvalidations() {
    // Allow any queued writes to go through first.
    FlushPendingWrites();
    return fake_tracker_.GetSavedInvalidations();
  }

  SingleObjectInvalidationSet GetSavedInvalidationsForType(const ObjectId& id) {
    const UnackedInvalidationsMap& saved_state = GetSavedInvalidations();
    auto it = saved_state.find(kBookmarksId_);
    if (it == saved_state.end()) {
      ADD_FAILURE() << "No state saved for ID " << ObjectIdToString(id);
      return SingleObjectInvalidationSet();
    }
    ObjectIdInvalidationMap map;
    it->second.ExportInvalidations(
        base::WeakPtr<AckHandler>(),
        scoped_refptr<base::SingleThreadTaskRunner>(),
        &map);
    if (map.Empty()) {
      return SingleObjectInvalidationSet();
    } else  {
      return map.ForObject(id);
    }
  }

  ObjectIdSet GetRegisteredIds() const {
    return fake_invalidation_client_->GetRegisteredIds();
  }

  // |payload| can be NULL.
  void FireInvalidate(const ObjectId& object_id,
                      int64_t version,
                      const char* payload) {
    invalidation::Invalidation inv;
    if (payload) {
      inv = invalidation::Invalidation(object_id, version, payload);
    } else {
      inv = invalidation::Invalidation(object_id, version);
    }
    const AckHandle ack_handle("fakedata");
    fake_invalidation_client_->ClearAckedHandles();
    listener_.Invalidate(fake_invalidation_client_, inv, ack_handle);
    EXPECT_TRUE(fake_invalidation_client_->IsAckedHandle(ack_handle));
  }

  // |payload| can be NULL, but not |type_name|.
  void FireInvalidateUnknownVersion(const ObjectId& object_id) {
    const AckHandle ack_handle("fakedata_unknown");
    fake_invalidation_client_->ClearAckedHandles();
    listener_.InvalidateUnknownVersion(fake_invalidation_client_,
                                       object_id,
                                       ack_handle);
    EXPECT_TRUE(fake_invalidation_client_->IsAckedHandle(ack_handle));
  }

  void FireInvalidateAll() {
    const AckHandle ack_handle("fakedata_all");
    fake_invalidation_client_->ClearAckedHandles();
    listener_.InvalidateAll(fake_invalidation_client_, ack_handle);
    EXPECT_TRUE(fake_invalidation_client_->IsAckedHandle(ack_handle));
  }

  void WriteState(const std::string& new_state) {
    listener_.WriteState(new_state);

    // Pump message loop to trigger
    // InvalidationStateTracker::WriteState().
    FlushPendingWrites();
  }

  void FlushPendingWrites() { base::RunLoop().RunUntilIdle(); }

  void EnableNotifications() {
    fake_push_client_->EnableNotifications();
  }

  void DisableNotifications(notifier::NotificationsDisabledReason reason) {
    fake_push_client_->DisableNotifications(reason);
  }

  const ObjectId kBookmarksId_;
  const ObjectId kPreferencesId_;
  const ObjectId kExtensionsId_;
  const ObjectId kAppsId_;

  ObjectIdSet registered_ids_;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  notifier::FakePushClient* const fake_push_client_;

 protected:
  // A derrived test needs direct access to this.
  FakeInvalidationStateTracker fake_tracker_;

  // Tests need to access these directly.
  FakeInvalidationClient* fake_invalidation_client_;
  SyncInvalidationListener listener_;

 private:
  FakeDelegate fake_delegate_;
};

// Write a new state to the client.  It should propagate to the
// tracker.
TEST_F(SyncInvalidationListenerTest, WriteState) {
  WriteState(kNewState);

  EXPECT_EQ(kNewState, GetBootstrapData());
}

// Invalidation tests.

// Fire an invalidation without a payload.  It should be processed,
// the payload should remain empty, and the version should be updated.
TEST_F(SyncInvalidationListenerTest, InvalidateNoPayload) {
  const ObjectId& id = kBookmarksId_;

  FireInvalidate(id, kVersion1, nullptr);

  ASSERT_EQ(1U, GetInvalidationCount(id));
  ASSERT_FALSE(IsUnknownVersion(id));
  EXPECT_EQ(kVersion1, GetVersion(id));
  EXPECT_EQ("", GetPayload(id));
}

// Fire an invalidation with an empty payload.  It should be
// processed, the payload should remain empty, and the version should
// be updated.
TEST_F(SyncInvalidationListenerTest, InvalidateEmptyPayload) {
  const ObjectId& id = kBookmarksId_;

  FireInvalidate(id, kVersion1, "");

  ASSERT_EQ(1U, GetInvalidationCount(id));
  ASSERT_FALSE(IsUnknownVersion(id));
  EXPECT_EQ(kVersion1, GetVersion(id));
  EXPECT_EQ("", GetPayload(id));
}

// Fire an invalidation with a payload.  It should be processed, and
// both the payload and the version should be updated.
TEST_F(SyncInvalidationListenerTest, InvalidateWithPayload) {
  const ObjectId& id = kPreferencesId_;

  FireInvalidate(id, kVersion1, kPayload1);

  ASSERT_EQ(1U, GetInvalidationCount(id));
  ASSERT_FALSE(IsUnknownVersion(id));
  EXPECT_EQ(kVersion1, GetVersion(id));
  EXPECT_EQ(kPayload1, GetPayload(id));
}

// Fire ten invalidations in a row.  All should be received.
TEST_F(SyncInvalidationListenerTest, ManyInvalidations_NoDrop) {
  const int kRepeatCount = 10;
  const ObjectId& id = kPreferencesId_;
  int64_t initial_version = kVersion1;
  for (int64_t i = initial_version; i < initial_version + kRepeatCount; ++i) {
    FireInvalidate(id, i, kPayload1);
  }
  ASSERT_EQ(static_cast<size_t>(kRepeatCount), GetInvalidationCount(id));
  ASSERT_FALSE(IsUnknownVersion(id));
  EXPECT_EQ(kPayload1, GetPayload(id));
  EXPECT_EQ(initial_version + kRepeatCount - 1, GetVersion(id));
}

// Fire an invalidation for an unregistered object ID with a payload.  It should
// still be processed, and both the payload and the version should be updated.
TEST_F(SyncInvalidationListenerTest, InvalidateBeforeRegistration_Simple) {
  const ObjectId kUnregisteredId(kChromeSyncSourceId, "unregistered");
  const ObjectId& id = kUnregisteredId;
  ObjectIdSet ids;
  ids.insert(id);

  EXPECT_EQ(0U, GetInvalidationCount(id));

  FireInvalidate(id, kVersion1, kPayload1);

  ASSERT_EQ(0U, GetInvalidationCount(id));

  EnableNotifications();
  listener_.Ready(fake_invalidation_client_);
  listener_.UpdateRegisteredIds(ids);

  ASSERT_EQ(1U, GetInvalidationCount(id));
  ASSERT_FALSE(IsUnknownVersion(id));
  EXPECT_EQ(kVersion1, GetVersion(id));
  EXPECT_EQ(kPayload1, GetPayload(id));
}

// Fire ten invalidations before an object registers.  Some invalidations will
// be dropped an replaced with an unknown version invalidation.
TEST_F(SyncInvalidationListenerTest, InvalidateBeforeRegistration_Drop) {
  const int kRepeatCount =
      UnackedInvalidationSet::kMaxBufferedInvalidations + 1;
  const ObjectId kUnregisteredId(kChromeSyncSourceId, "unregistered");
  const ObjectId& id = kUnregisteredId;
  ObjectIdSet ids;
  ids.insert(id);

  EXPECT_EQ(0U, GetInvalidationCount(id));

  int64_t initial_version = kVersion1;
  for (int64_t i = initial_version; i < initial_version + kRepeatCount; ++i) {
    FireInvalidate(id, i, kPayload1);
  }

  EnableNotifications();
  listener_.Ready(fake_invalidation_client_);
  listener_.UpdateRegisteredIds(ids);

  ASSERT_EQ(UnackedInvalidationSet::kMaxBufferedInvalidations,
            GetInvalidationCount(id));
  ASSERT_FALSE(IsUnknownVersion(id));
  EXPECT_EQ(initial_version + kRepeatCount - 1, GetVersion(id));
  EXPECT_EQ(kPayload1, GetPayload(id));
  EXPECT_TRUE(StartsWithUnknownVersion(id));
}

// Fire an invalidation, then fire another one with a lower version.  Both
// should be received.
TEST_F(SyncInvalidationListenerTest, InvalidateVersion) {
  const ObjectId& id = kPreferencesId_;

  FireInvalidate(id, kVersion2, kPayload2);

  ASSERT_EQ(1U, GetInvalidationCount(id));
  ASSERT_FALSE(IsUnknownVersion(id));
  EXPECT_EQ(kVersion2, GetVersion(id));
  EXPECT_EQ(kPayload2, GetPayload(id));

  FireInvalidate(id, kVersion1, kPayload1);

  ASSERT_EQ(2U, GetInvalidationCount(id));
  ASSERT_FALSE(IsUnknownVersion(id));

  EXPECT_EQ(kVersion1, GetVersion(id));
  EXPECT_EQ(kPayload1, GetPayload(id));
}

// Fire an invalidation with an unknown version.
TEST_F(SyncInvalidationListenerTest, InvalidateUnknownVersion) {
  const ObjectId& id = kBookmarksId_;

  FireInvalidateUnknownVersion(id);

  ASSERT_EQ(1U, GetInvalidationCount(id));
  EXPECT_TRUE(IsUnknownVersion(id));
}

// Fire an invalidation for all enabled IDs.
TEST_F(SyncInvalidationListenerTest, InvalidateAll) {
  FireInvalidateAll();

  for (auto it = registered_ids_.begin(); it != registered_ids_.end(); ++it) {
    ASSERT_EQ(1U, GetInvalidationCount(*it));
    EXPECT_TRUE(IsUnknownVersion(*it));
  }
}

// Test a simple scenario for multiple IDs.
TEST_F(SyncInvalidationListenerTest, InvalidateMultipleIds) {
  FireInvalidate(kBookmarksId_, 3, nullptr);

  ASSERT_EQ(1U, GetInvalidationCount(kBookmarksId_));
  ASSERT_FALSE(IsUnknownVersion(kBookmarksId_));
  EXPECT_EQ(3, GetVersion(kBookmarksId_));
  EXPECT_EQ("", GetPayload(kBookmarksId_));

  // kExtensionId is not registered, so the invalidation should not get through.
  FireInvalidate(kExtensionsId_, 2, nullptr);
  ASSERT_EQ(0U, GetInvalidationCount(kExtensionsId_));
}

// Registration tests.

// With IDs already registered, enable notifications then ready the
// client.  The IDs should be registered only after the client is
// readied.
TEST_F(SyncInvalidationListenerTest, RegisterEnableReady) {
  EXPECT_TRUE(GetRegisteredIds().empty());

  EnableNotifications();

  EXPECT_TRUE(GetRegisteredIds().empty());

  listener_.Ready(fake_invalidation_client_);

  EXPECT_EQ(registered_ids_, GetRegisteredIds());
}

// With IDs already registered, ready the client then enable
// notifications.  The IDs should be registered after the client is
// readied.
TEST_F(SyncInvalidationListenerTest, RegisterReadyEnable) {
  EXPECT_TRUE(GetRegisteredIds().empty());

  listener_.Ready(fake_invalidation_client_);

  EXPECT_EQ(registered_ids_, GetRegisteredIds());

  EnableNotifications();

  EXPECT_EQ(registered_ids_, GetRegisteredIds());
}

// Unregister the IDs, enable notifications, re-register the IDs, then
// ready the client.  The IDs should be registered only after the
// client is readied.
TEST_F(SyncInvalidationListenerTest, EnableRegisterReady) {
  listener_.UpdateRegisteredIds(ObjectIdSet());

  EXPECT_TRUE(GetRegisteredIds().empty());

  EnableNotifications();

  EXPECT_TRUE(GetRegisteredIds().empty());

  listener_.UpdateRegisteredIds(registered_ids_);

  EXPECT_TRUE(GetRegisteredIds().empty());

  listener_.Ready(fake_invalidation_client_);

  EXPECT_EQ(registered_ids_, GetRegisteredIds());
}

// Unregister the IDs, enable notifications, ready the client, then
// re-register the IDs.  The IDs should be registered only after the
// client is readied.
TEST_F(SyncInvalidationListenerTest, EnableReadyRegister) {
  listener_.UpdateRegisteredIds(ObjectIdSet());

  EXPECT_TRUE(GetRegisteredIds().empty());

  EnableNotifications();

  EXPECT_TRUE(GetRegisteredIds().empty());

  listener_.Ready(fake_invalidation_client_);

  EXPECT_TRUE(GetRegisteredIds().empty());

  listener_.UpdateRegisteredIds(registered_ids_);

  EXPECT_EQ(registered_ids_, GetRegisteredIds());
}

// Unregister the IDs, ready the client, enable notifications, then
// re-register the IDs.  The IDs should be registered only after the
// client is readied.
TEST_F(SyncInvalidationListenerTest, ReadyEnableRegister) {
  listener_.UpdateRegisteredIds(ObjectIdSet());

  EXPECT_TRUE(GetRegisteredIds().empty());

  EnableNotifications();

  EXPECT_TRUE(GetRegisteredIds().empty());

  listener_.Ready(fake_invalidation_client_);

  EXPECT_TRUE(GetRegisteredIds().empty());

  listener_.UpdateRegisteredIds(registered_ids_);

  EXPECT_EQ(registered_ids_, GetRegisteredIds());
}

// Unregister the IDs, ready the client, re-register the IDs, then
// enable notifications. The IDs should be registered only after the
// client is readied.
//
// This test is important: see http://crbug.com/139424.
TEST_F(SyncInvalidationListenerTest, ReadyRegisterEnable) {
  listener_.UpdateRegisteredIds(ObjectIdSet());

  EXPECT_TRUE(GetRegisteredIds().empty());

  listener_.Ready(fake_invalidation_client_);

  EXPECT_TRUE(GetRegisteredIds().empty());

  listener_.UpdateRegisteredIds(registered_ids_);

  EXPECT_EQ(registered_ids_, GetRegisteredIds());

  EnableNotifications();

  EXPECT_EQ(registered_ids_, GetRegisteredIds());
}

// With IDs already registered, ready the client, restart the client,
// then re-ready it.  The IDs should still be registered.
TEST_F(SyncInvalidationListenerTest, RegisterTypesPreserved) {
  EXPECT_TRUE(GetRegisteredIds().empty());

  listener_.Ready(fake_invalidation_client_);

  EXPECT_EQ(registered_ids_, GetRegisteredIds());

  RestartClient();

  EXPECT_TRUE(GetRegisteredIds().empty());

  listener_.Ready(fake_invalidation_client_);

  EXPECT_EQ(registered_ids_, GetRegisteredIds());
}

// Make sure that state is correctly purged from the local invalidation state
// map cache when an ID is unregistered.
TEST_F(SyncInvalidationListenerTest, UnregisterCleansUpStateMapCache) {
  const ObjectId& id = kBookmarksId_;
  listener_.Ready(fake_invalidation_client_);

  EXPECT_TRUE(GetSavedInvalidations().empty());
  FireInvalidate(id, 1, "hello");
  EXPECT_EQ(1U, GetSavedInvalidations().size());
  EXPECT_TRUE(base::Contains(GetSavedInvalidations(), id));
  FireInvalidate(kPreferencesId_, 2, "world");
  EXPECT_EQ(2U, GetSavedInvalidations().size());

  EXPECT_TRUE(base::Contains(GetSavedInvalidations(), id));
  EXPECT_TRUE(base::Contains(GetSavedInvalidations(), kPreferencesId_));

  ObjectIdSet ids;
  ids.insert(id);
  listener_.UpdateRegisteredIds(ids);
  EXPECT_EQ(1U, GetSavedInvalidations().size());
  EXPECT_TRUE(base::Contains(GetSavedInvalidations(), id));
}

TEST_F(SyncInvalidationListenerTest, DuplicateInvaldiations_Simple) {
  const ObjectId& id = kBookmarksId_;
  listener_.Ready(fake_invalidation_client_);

  // Send a stream of invalidations, including two copies of the second.
  FireInvalidate(id, 1, "one");
  FireInvalidate(id, 2, "two");
  FireInvalidate(id, 3, "three");
  FireInvalidate(id, 2, "two");

  // Expect that the duplicate was discarded.
  SingleObjectInvalidationSet list = GetSavedInvalidationsForType(id);
  EXPECT_EQ(3U, list.GetSize());
  auto it = list.begin();
  EXPECT_EQ(1, it->version());
  it++;
  EXPECT_EQ(2, it->version());
  it++;
  EXPECT_EQ(3, it->version());
}

TEST_F(SyncInvalidationListenerTest, DuplicateInvalidations_NearBufferLimit) {
  const size_t kPairsToSend = UnackedInvalidationSet::kMaxBufferedInvalidations;
  const ObjectId& id = kBookmarksId_;
  listener_.Ready(fake_invalidation_client_);

  // We will have enough buffer space in the state tracker for all these
  // invalidations only if duplicates are ignored.
  for (size_t i = 0; i < kPairsToSend; ++i) {
    FireInvalidate(id, i, "payload");
    FireInvalidate(id, i, "payload");
  }

  // Expect that the state map ignored duplicates.
  SingleObjectInvalidationSet list = GetSavedInvalidationsForType(id);
  EXPECT_EQ(kPairsToSend, list.GetSize());
  EXPECT_FALSE(list.begin()->is_unknown_version());

  // Expect that all invalidations (including duplicates) were emitted.
  EXPECT_EQ(kPairsToSend*2, GetInvalidationCount(id));

  // Acknowledge all invalidations to clear the internal state.
  AcknowledgeAll(id);
  EXPECT_TRUE(GetSavedInvalidationsForType(id).IsEmpty());
}

TEST_F(SyncInvalidationListenerTest, DuplicateInvalidations_UnknownVersion) {
  const ObjectId& id = kBookmarksId_;
  listener_.Ready(fake_invalidation_client_);

  FireInvalidateUnknownVersion(id);
  FireInvalidateUnknownVersion(id);

  {
    SingleObjectInvalidationSet list = GetSavedInvalidationsForType(id);
    EXPECT_EQ(1U, list.GetSize());
  }

  // Acknowledge the second.  There should be no effect on the stored list.
  ASSERT_EQ(2U, GetInvalidationCount(id));
  AcknowledgeNthInvalidation(id, 1);
  {
    SingleObjectInvalidationSet list = GetSavedInvalidationsForType(id);
    EXPECT_EQ(1U, list.GetSize());
  }

  // Acknowledge the first.  This should remove the invalidation from the list.
  ASSERT_EQ(2U, GetInvalidationCount(id));
  AcknowledgeNthInvalidation(id, 0);
  {
    SingleObjectInvalidationSet list = GetSavedInvalidationsForType(id);
    EXPECT_EQ(0U, list.GetSize());
  }
}

// Make sure that acknowledgements erase items from the local store.
TEST_F(SyncInvalidationListenerTest, AcknowledgementsCleanUpStateMapCache) {
  const ObjectId& id = kBookmarksId_;
  listener_.Ready(fake_invalidation_client_);

  EXPECT_TRUE(GetSavedInvalidations().empty());
  FireInvalidate(id, 10, "hello");
  FireInvalidate(id, 20, "world");
  FireInvalidateUnknownVersion(id);

  // Expect that all three invalidations have been saved to permanent storage.
  {
    SingleObjectInvalidationSet list = GetSavedInvalidationsForType(id);
    ASSERT_EQ(3U, list.GetSize());
    EXPECT_TRUE(list.begin()->is_unknown_version());
    EXPECT_EQ(20, list.back().version());
  }

  // Acknowledge the second sent invaldiation (version 20) and verify it was
  // removed from storage.
  AcknowledgeNthInvalidation(id, 1);
  {
    SingleObjectInvalidationSet list = GetSavedInvalidationsForType(id);
    ASSERT_EQ(2U, list.GetSize());
    EXPECT_TRUE(list.begin()->is_unknown_version());
    EXPECT_EQ(10, list.back().version());
  }

  // Acknowledge the last sent invalidation (unknown version) and verify it was
  // removed from storage.
  AcknowledgeNthInvalidation(id, 2);
  {
    SingleObjectInvalidationSet list = GetSavedInvalidationsForType(id);
    ASSERT_EQ(1U, list.GetSize());
    EXPECT_FALSE(list.begin()->is_unknown_version());
    EXPECT_EQ(10, list.back().version());
  }
}

// Make sure that drops erase items from the local store.
TEST_F(SyncInvalidationListenerTest, DropsCleanUpStateMapCache) {
  const ObjectId& id = kBookmarksId_;
  listener_.Ready(fake_invalidation_client_);

  EXPECT_TRUE(GetSavedInvalidations().empty());
  FireInvalidate(id, 10, "hello");
  FireInvalidate(id, 20, "world");
  FireInvalidateUnknownVersion(id);

  // Expect that all three invalidations have been saved to permanent storage.
  {
    SingleObjectInvalidationSet list = GetSavedInvalidationsForType(id);
    ASSERT_EQ(3U, list.GetSize());
    EXPECT_TRUE(list.begin()->is_unknown_version());
    EXPECT_EQ(20, list.back().version());
  }

  // Drop the second sent invalidation (version 20) and verify it was removed
  // from storage.  Also verify we still have an unknown version invalidation.
  DropNthInvalidation(id, 1);
  {
    SingleObjectInvalidationSet list = GetSavedInvalidationsForType(id);
    ASSERT_EQ(2U, list.GetSize());
    EXPECT_TRUE(list.begin()->is_unknown_version());
    EXPECT_EQ(10, list.back().version());
  }

  // Drop the remaining invalidation.  Verify an unknown version is all that
  // remains.
  DropNthInvalidation(id, 0);
  {
    SingleObjectInvalidationSet list = GetSavedInvalidationsForType(id);
    ASSERT_EQ(1U, list.GetSize());
    EXPECT_TRUE(list.begin()->is_unknown_version());
  }

  // Announce that the delegate has recovered from the drop.  Verify no
  // invalidations remain saved.
  RecoverFromDropEvent(id);
  EXPECT_TRUE(GetSavedInvalidationsForType(id).IsEmpty());

  RecoverFromDropEvent(id);
}

// Without readying the client, disable notifications, then enable
// them.  The listener should still think notifications are disabled.
TEST_F(SyncInvalidationListenerTest, EnableNotificationsNotReady) {
  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR,
            GetInvalidatorState());

  DisableNotifications(
      notifier::TRANSIENT_NOTIFICATION_ERROR);

  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, GetInvalidatorState());

  DisableNotifications(notifier::NOTIFICATION_CREDENTIALS_REJECTED);

  EXPECT_EQ(INVALIDATION_CREDENTIALS_REJECTED, GetInvalidatorState());

  EnableNotifications();

  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, GetInvalidatorState());
}

// Enable notifications then Ready the invalidation client.  The
// delegate should then be ready.
TEST_F(SyncInvalidationListenerTest, EnableNotificationsThenReady) {
  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, GetInvalidatorState());

  EnableNotifications();

  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, GetInvalidatorState());

  listener_.Ready(fake_invalidation_client_);

  EXPECT_EQ(INVALIDATIONS_ENABLED, GetInvalidatorState());
}

// Ready the invalidation client then enable notifications.  The
// delegate should then be ready.
TEST_F(SyncInvalidationListenerTest, ReadyThenEnableNotifications) {
  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, GetInvalidatorState());

  listener_.Ready(fake_invalidation_client_);

  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, GetInvalidatorState());

  EnableNotifications();

  EXPECT_EQ(INVALIDATIONS_ENABLED, GetInvalidatorState());
}

// Enable notifications and ready the client.  Then disable
// notifications with an auth error and re-enable notifications.  The
// delegate should go into an auth error mode and then back out.
TEST_F(SyncInvalidationListenerTest, PushClientAuthError) {
  EnableNotifications();
  listener_.Ready(fake_invalidation_client_);

  EXPECT_EQ(INVALIDATIONS_ENABLED, GetInvalidatorState());

  DisableNotifications(
      notifier::NOTIFICATION_CREDENTIALS_REJECTED);

  EXPECT_EQ(INVALIDATION_CREDENTIALS_REJECTED, GetInvalidatorState());

  EnableNotifications();

  EXPECT_EQ(INVALIDATIONS_ENABLED, GetInvalidatorState());
}

// Enable notifications and ready the client.  Then simulate an auth
// error from the invalidation client.  Simulate some notification
// events, then re-ready the client.  The delegate should go into an
// auth error mode and come out of it only after the client is ready.
TEST_F(SyncInvalidationListenerTest, InvalidationClientAuthError) {
  EnableNotifications();
  listener_.Ready(fake_invalidation_client_);

  EXPECT_EQ(INVALIDATIONS_ENABLED, GetInvalidatorState());

  listener_.InformError(
      fake_invalidation_client_,
      invalidation::ErrorInfo(
          invalidation::ErrorReason::AUTH_FAILURE,
          false /* is_transient */,
          "auth error",
          invalidation::ErrorContext()));

  EXPECT_EQ(INVALIDATION_CREDENTIALS_REJECTED, GetInvalidatorState());

  DisableNotifications(notifier::TRANSIENT_NOTIFICATION_ERROR);

  EXPECT_EQ(INVALIDATION_CREDENTIALS_REJECTED, GetInvalidatorState());

  DisableNotifications(notifier::TRANSIENT_NOTIFICATION_ERROR);

  EXPECT_EQ(INVALIDATION_CREDENTIALS_REJECTED, GetInvalidatorState());

  EnableNotifications();

  EXPECT_EQ(INVALIDATION_CREDENTIALS_REJECTED, GetInvalidatorState());

  listener_.Ready(fake_invalidation_client_);

  EXPECT_EQ(INVALIDATIONS_ENABLED, GetInvalidatorState());
}

// A variant of SyncInvalidationListenerTest that starts with some initial
// state.  We make not attempt to abstract away the contents of this state.  The
// tests that make use of this harness depend on its implementation details.
class SyncInvalidationListenerTest_WithInitialState
    : public SyncInvalidationListenerTest {
 public:
  void SetUp() override {
    UnackedInvalidationSet bm_state(kBookmarksId_);
    UnackedInvalidationSet ext_state(kExtensionsId_);

    Invalidation bm_unknown = Invalidation::InitUnknownVersion(kBookmarksId_);
    Invalidation bm_v100 = Invalidation::Init(kBookmarksId_, 100, "hundred");
    bm_state.Add(bm_unknown);
    bm_state.Add(bm_v100);

    Invalidation ext_v10 = Invalidation::Init(kExtensionsId_, 10, "ten");
    Invalidation ext_v20 = Invalidation::Init(kExtensionsId_, 20, "twenty");
    ext_state.Add(ext_v10);
    ext_state.Add(ext_v20);

    initial_state.insert(std::make_pair(kBookmarksId_, bm_state));
    initial_state.insert(std::make_pair(kExtensionsId_, ext_state));

    fake_tracker_.SetSavedInvalidations(initial_state);

    SyncInvalidationListenerTest::SetUp();
  }

  UnackedInvalidationsMap initial_state;
};

// Verify that saved invalidations are forwarded when handlers register.
TEST_F(SyncInvalidationListenerTest_WithInitialState,
       ReceiveSavedInvalidations) {
  EnableNotifications();
  listener_.Ready(fake_invalidation_client_);

  EXPECT_THAT(initial_state, test_util::Eq(GetSavedInvalidations()));

  ASSERT_EQ(2U, GetInvalidationCount(kBookmarksId_));
  EXPECT_EQ(100, GetVersion(kBookmarksId_));

  ASSERT_EQ(0U, GetInvalidationCount(kExtensionsId_));

  FireInvalidate(kExtensionsId_, 30, "thirty");

  ObjectIdSet ids = GetRegisteredIds();
  ids.insert(kExtensionsId_);
  listener_.UpdateRegisteredIds(ids);

  ASSERT_EQ(3U, GetInvalidationCount(kExtensionsId_));
  EXPECT_EQ(30, GetVersion(kExtensionsId_));
}

}  // namespace

}  // namespace syncer
