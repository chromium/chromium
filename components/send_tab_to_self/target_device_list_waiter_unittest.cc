// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/target_device_list_waiter.h"

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/send_tab_to_self/entry_point_display_reason.h"
#include "components/send_tab_to_self/stub_send_tab_to_self_sync_service.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace send_tab_to_self {

namespace {

using base::test::TestFuture;
using testing::IsNull;
using testing::Test;

const char kTestUrl[] = "https://www.example.com";

class TargetDeviceListWaiterTest : public Test {
 public:
  TargetDeviceListWaiterTest() = default;

  syncer::TestSyncService* sync_service() { return &sync_service_; }
  StubSendTabToSelfSyncService* send_tab_to_self_service() {
    return &send_tab_to_self_service_;
  }

  void SetDisplayReason(std::optional<EntryPointDisplayReason> reason) {
    send_tab_to_self_service_.SetEntryPointDisplayReason(reason);
  }

 private:
  base::test::TaskEnvironment task_environment_;
  syncer::TestSyncService sync_service_;
  StubSendTabToSelfSyncService send_tab_to_self_service_;
};

// Verifies that the waiter triggers its completion callback when the display
// reason transitions to kOfferFeature.
TEST_F(TargetDeviceListWaiterTest,
       TriggersCallbackWhenDisplayReasonIsOfferFeature) {
  TestFuture<void> future;
  SetDisplayReason(EntryPointDisplayReason::kOfferSignIn);

  TargetDeviceListWaiter waiter(sync_service(), send_tab_to_self_service(),
                                GURL(kTestUrl), future.GetCallback());

  EXPECT_FALSE(future.IsReady());

  // Transition display reason to kOfferFeature and notify sync state change.
  SetDisplayReason(EntryPointDisplayReason::kOfferFeature);
  sync_service()->FireStateChanged();

  EXPECT_TRUE(future.Wait());
}

// Verifies that the waiter triggers its completion callback when the display
// reason transitions to kInformNoTargetDevice.
TEST_F(TargetDeviceListWaiterTest,
       TriggersCallbackWhenDisplayReasonIsInformNoTargetDevice) {
  TestFuture<void> future;
  SetDisplayReason(EntryPointDisplayReason::kOfferSignIn);

  TargetDeviceListWaiter waiter(sync_service(), send_tab_to_self_service(),
                                GURL(kTestUrl), future.GetCallback());

  EXPECT_FALSE(future.IsReady());

  // Transition display reason to kInformNoTargetDevice and notify sync state
  // change.
  SetDisplayReason(EntryPointDisplayReason::kInformNoTargetDevice);
  sync_service()->FireStateChanged();

  EXPECT_TRUE(future.Wait());
}

// Verifies that the waiter remains waiting while display reason is kOfferSignIn
// or nullopt, and completes when it eventually resolves to kOfferFeature.
TEST_F(TargetDeviceListWaiterTest,
       DoesNotTriggerCallbackWhileDisplayReasonIsPending) {
  TestFuture<void> future;
  SetDisplayReason(std::nullopt);

  TargetDeviceListWaiter waiter(sync_service(), send_tab_to_self_service(),
                                GURL(kTestUrl), future.GetCallback());

  EXPECT_FALSE(future.IsReady());

  // Set to kOfferSignIn - still waiting.
  SetDisplayReason(EntryPointDisplayReason::kOfferSignIn);
  sync_service()->FireStateChanged();

  EXPECT_FALSE(future.IsReady());

  // Transition to kOfferFeature - completes successfully.
  SetDisplayReason(EntryPointDisplayReason::kOfferFeature);
  sync_service()->FireStateChanged();

  EXPECT_TRUE(future.Wait());
}

// Verifies that the waiter remains waiting when display reason is kOfferReauth
// and resolves once it transitions to kInformNoTargetDevice.
TEST_F(TargetDeviceListWaiterTest,
       DoesNotTriggerCallbackWhileDisplayReasonIsOfferReauth) {
  TestFuture<void> future;
  SetDisplayReason(EntryPointDisplayReason::kOfferSignIn);

  TargetDeviceListWaiter waiter(sync_service(), send_tab_to_self_service(),
                                GURL(kTestUrl), future.GetCallback());

  EXPECT_FALSE(future.IsReady());

  // Set to kOfferReauth - still waiting.
  SetDisplayReason(EntryPointDisplayReason::kOfferReauth);
  sync_service()->FireStateChanged();

  EXPECT_FALSE(future.IsReady());

  // Transition to kInformNoTargetDevice - completes successfully.
  SetDisplayReason(EntryPointDisplayReason::kInformNoTargetDevice);
  sync_service()->FireStateChanged();

  EXPECT_TRUE(future.Wait());
}

// Verifies that if the display reason is already resolved at construction
// time, the callback triggers synchronously in the constructor.
TEST_F(TargetDeviceListWaiterTest,
       TriggersCallbackImmediatelyIfAlreadyResolvedAtConstruction) {
  TestFuture<void> future;
  SetDisplayReason(EntryPointDisplayReason::kOfferFeature);

  TargetDeviceListWaiter waiter(sync_service(), send_tab_to_self_service(),
                                GURL(kTestUrl), future.GetCallback());

  EXPECT_TRUE(future.IsReady());
}

// Verifies that null SyncService pointers are handled gracefully without
// crashing when display reason is pending.
TEST_F(TargetDeviceListWaiterTest, HandlesNullSyncServiceGracefully) {
  TestFuture<void> future;
  SetDisplayReason(EntryPointDisplayReason::kOfferSignIn);

  TargetDeviceListWaiter waiter(
      /*sync_service=*/nullptr, send_tab_to_self_service(), GURL(kTestUrl),
      future.GetCallback());

  EXPECT_FALSE(future.IsReady());
}

// Verifies that null SendTabToSelfSyncService pointers are handled gracefully
// without crashing.
TEST_F(TargetDeviceListWaiterTest, HandlesNullSendTabToSelfServiceGracefully) {
  TestFuture<void> future;

  TargetDeviceListWaiter waiter(
      sync_service(), /*send_tab_to_self_service=*/nullptr, GURL(kTestUrl),
      future.GetCallback());

  EXPECT_FALSE(future.IsReady());

  // Subsequent sync state changes should not crash when service is null.
  sync_service()->FireStateChanged();
  EXPECT_FALSE(future.IsReady());
}

// Verifies that deleting the waiter inside its own completion callback is safe
// and does not cause a crash or use-after-free.
TEST_F(TargetDeviceListWaiterTest, HandlesSelfDestructionInCompletionCallback) {
  SetDisplayReason(EntryPointDisplayReason::kOfferSignIn);

  std::unique_ptr<TargetDeviceListWaiter> waiter;
  waiter = std::make_unique<TargetDeviceListWaiter>(
      sync_service(), send_tab_to_self_service(), GURL(kTestUrl),
      base::BindOnce(
          [](std::unique_ptr<TargetDeviceListWaiter>* waiter_ptr) {
            waiter_ptr->reset();
          },
          &waiter));

  SetDisplayReason(EntryPointDisplayReason::kOfferFeature);
  sync_service()->FireStateChanged();

  EXPECT_THAT(waiter, IsNull());
}

// Verifies that sync shutdown resets observation without crashing or firing
// callback prematurely.
TEST_F(TargetDeviceListWaiterTest, HandlesSyncShutdownWithoutCrashing) {
  TestFuture<void> future;
  SetDisplayReason(EntryPointDisplayReason::kOfferSignIn);

  TargetDeviceListWaiter waiter(sync_service(), send_tab_to_self_service(),
                                GURL(kTestUrl), future.GetCallback());

  waiter.OnSyncShutdown(sync_service());

  // State changes after shutdown should be ignored.
  SetDisplayReason(EntryPointDisplayReason::kOfferFeature);
  sync_service()->FireStateChanged();

  EXPECT_FALSE(future.IsReady());
}

// Verifies that subsequent sync state change notifications after resolution
// do not crash or re-trigger completion.
TEST_F(TargetDeviceListWaiterTest,
       MultipleStateChangesDoNotCrashOrReTriggerCallback) {
  int callback_count = 0;
  SetDisplayReason(EntryPointDisplayReason::kOfferSignIn);

  TargetDeviceListWaiter waiter(
      sync_service(), send_tab_to_self_service(), GURL(kTestUrl),
      base::BindLambdaForTesting([&]() { callback_count++; }));

  SetDisplayReason(EntryPointDisplayReason::kOfferFeature);
  // Trigger state change twice in succession.
  sync_service()->FireStateChanged();
  sync_service()->FireStateChanged();

  EXPECT_EQ(callback_count, 1);
}

}  // namespace

}  // namespace send_tab_to_self
