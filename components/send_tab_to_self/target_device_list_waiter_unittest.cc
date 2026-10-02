// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/target_device_list_waiter.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
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
using testing::Eq;
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

  // Posts a sentinel task and waits for it, ensuring all previously posted
  // tasks have run.
  void FlushPostedTasks() {
    base::RunLoop run_loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
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
  send_tab_to_self_service()->SetEntryPointDisplayReason(
      EntryPointDisplayReason::kOfferSignIn);

  TargetDeviceListWaiter waiter(sync_service(), send_tab_to_self_service(),
                                GURL(kTestUrl), future.GetCallback());

  EXPECT_FALSE(future.IsReady());

  // Transition display reason to kOfferFeature and notify sync state change.
  send_tab_to_self_service()->SetEntryPointDisplayReason(
      EntryPointDisplayReason::kOfferFeature);
  sync_service()->FireStateChanged();

  EXPECT_TRUE(future.Wait());
}

// Verifies that the waiter triggers its completion callback when the display
// reason transitions to kInformNoTargetDevice.
TEST_F(TargetDeviceListWaiterTest,
       TriggersCallbackWhenDisplayReasonIsInformNoTargetDevice) {
  TestFuture<void> future;
  send_tab_to_self_service()->SetEntryPointDisplayReason(
      EntryPointDisplayReason::kOfferSignIn);

  TargetDeviceListWaiter waiter(sync_service(), send_tab_to_self_service(),
                                GURL(kTestUrl), future.GetCallback());

  EXPECT_FALSE(future.IsReady());

  // Transition display reason to kInformNoTargetDevice and notify sync state
  // change.
  send_tab_to_self_service()->SetEntryPointDisplayReason(
      EntryPointDisplayReason::kInformNoTargetDevice);
  sync_service()->FireStateChanged();

  EXPECT_TRUE(future.Wait());
}

// Verifies that the waiter remains waiting while display reason is kOfferSignIn
// or nullopt, and completes when it eventually resolves to kOfferFeature.
TEST_F(TargetDeviceListWaiterTest,
       DoesNotTriggerCallbackWhileDisplayReasonIsPending) {
  TestFuture<void> future;
  send_tab_to_self_service()->SetEntryPointDisplayReason(std::nullopt);

  TargetDeviceListWaiter waiter(sync_service(), send_tab_to_self_service(),
                                GURL(kTestUrl), future.GetCallback());

  EXPECT_FALSE(future.IsReady());

  // Set to kOfferSignIn - still waiting.
  send_tab_to_self_service()->SetEntryPointDisplayReason(
      EntryPointDisplayReason::kOfferSignIn);
  sync_service()->FireStateChanged();

  EXPECT_FALSE(future.IsReady());

  // Transition to kOfferFeature - completes successfully.
  send_tab_to_self_service()->SetEntryPointDisplayReason(
      EntryPointDisplayReason::kOfferFeature);
  sync_service()->FireStateChanged();

  EXPECT_TRUE(future.Wait());
}

// Verifies that the waiter remains waiting when display reason is kOfferReauth
// and resolves once it transitions to kInformNoTargetDevice.
TEST_F(TargetDeviceListWaiterTest,
       DoesNotTriggerCallbackWhileDisplayReasonIsOfferReauth) {
  TestFuture<void> future;
  send_tab_to_self_service()->SetEntryPointDisplayReason(
      EntryPointDisplayReason::kOfferSignIn);

  TargetDeviceListWaiter waiter(sync_service(), send_tab_to_self_service(),
                                GURL(kTestUrl), future.GetCallback());

  EXPECT_FALSE(future.IsReady());

  // Set to kOfferReauth - still waiting.
  send_tab_to_self_service()->SetEntryPointDisplayReason(
      EntryPointDisplayReason::kOfferReauth);
  sync_service()->FireStateChanged();

  EXPECT_FALSE(future.IsReady());

  // Transition to kInformNoTargetDevice - completes successfully.
  send_tab_to_self_service()->SetEntryPointDisplayReason(
      EntryPointDisplayReason::kInformNoTargetDevice);
  sync_service()->FireStateChanged();

  EXPECT_TRUE(future.Wait());
}

// Verifies that if the display reason is already resolved at construction
// time, the callback triggers asynchronously rather than synchronously in the
// constructor.
TEST_F(TargetDeviceListWaiterTest,
       TriggersCallbackAsynchronouslyIfAlreadyResolvedAtConstruction) {
  TestFuture<void> future;
  send_tab_to_self_service()->SetEntryPointDisplayReason(
      EntryPointDisplayReason::kOfferFeature);

  TargetDeviceListWaiter waiter(sync_service(), send_tab_to_self_service(),
                                GURL(kTestUrl), future.GetCallback());

  EXPECT_FALSE(future.IsReady());
  EXPECT_TRUE(future.Wait());
}

// Verifies that destroying the waiter before the posted callback has executed
// cancels the callback execution.
TEST_F(TargetDeviceListWaiterTest, DestroyingWaiterCancelsCallback) {
  TestFuture<void> future;
  send_tab_to_self_service()->SetEntryPointDisplayReason(
      EntryPointDisplayReason::kOfferFeature);

  {
    TargetDeviceListWaiter waiter(sync_service(), send_tab_to_self_service(),
                                  GURL(kTestUrl), future.GetCallback());
    EXPECT_FALSE(future.IsReady());
    // `waiter` is destroyed at the end of this scope.
  }

  FlushPostedTasks();
  EXPECT_FALSE(future.IsReady());
}

// Verifies that deleting the waiter inside its own completion callback is safe
// and does not cause a crash or use-after-free.
TEST_F(TargetDeviceListWaiterTest, HandlesSelfDestructionInCompletionCallback) {
  TestFuture<void> future;
  send_tab_to_self_service()->SetEntryPointDisplayReason(
      EntryPointDisplayReason::kOfferSignIn);

  std::unique_ptr<TargetDeviceListWaiter> waiter;
  waiter = std::make_unique<TargetDeviceListWaiter>(
      sync_service(), send_tab_to_self_service(), GURL(kTestUrl),
      base::BindLambdaForTesting(
          [&waiter, callback = future.GetCallback()]() mutable {
            waiter.reset();
            std::move(callback).Run();
          }));

  send_tab_to_self_service()->SetEntryPointDisplayReason(
      EntryPointDisplayReason::kOfferFeature);
  sync_service()->FireStateChanged();

  EXPECT_TRUE(future.Wait());
  EXPECT_THAT(waiter, IsNull());
}

// Verifies that sync shutdown resets observation without crashing or firing
// callback prematurely.
TEST_F(TargetDeviceListWaiterTest, HandlesSyncShutdownWithoutCrashing) {
  TestFuture<void> future;
  send_tab_to_self_service()->SetEntryPointDisplayReason(
      EntryPointDisplayReason::kOfferSignIn);

  TargetDeviceListWaiter waiter(sync_service(), send_tab_to_self_service(),
                                GURL(kTestUrl), future.GetCallback());

  waiter.OnSyncShutdown(sync_service());

  // State changes and model notifications after shutdown should not crash or
  // run the callback.
  send_tab_to_self_service()->SetEntryPointDisplayReason(
      EntryPointDisplayReason::kOfferFeature);
  sync_service()->FireStateChanged();
  send_tab_to_self_service()->GetFakeSendTabToSelfModel()->SetIsReady(true);

  FlushPostedTasks();
  EXPECT_FALSE(future.IsReady());
}

// Verifies that subsequent SyncService state changes and model ready
// notifications after resolution do not crash or re-trigger completion.
TEST_F(TargetDeviceListWaiterTest,
       MultipleNotificationsDoNotCrashOrReTriggerCallback) {
  TestFuture<void> future;
  int callback_count = 0;
  send_tab_to_self_service()->SetEntryPointDisplayReason(
      EntryPointDisplayReason::kOfferSignIn);

  TargetDeviceListWaiter waiter(sync_service(), send_tab_to_self_service(),
                                GURL(kTestUrl),
                                base::BindLambdaForTesting([&]() {
                                  callback_count++;
                                  future.SetValue();
                                }));

  send_tab_to_self_service()->SetEntryPointDisplayReason(
      EntryPointDisplayReason::kOfferFeature);
  // Trigger state change twice in succession.
  sync_service()->FireStateChanged();
  sync_service()->FireStateChanged();

  EXPECT_TRUE(future.Wait());

  // Model notifications after resolution should not crash or re-run the
  // callback.
  send_tab_to_self_service()->GetFakeSendTabToSelfModel()->SetIsReady(true);

  FlushPostedTasks();
  EXPECT_THAT(callback_count, Eq(1));
}

// Verifies that the waiter triggers its completion callback when the model
// notifies OnModelReady() (e.g. after DeviceInfo finishes loading from disk)
// even without a separate SyncService state change.
TEST_F(TargetDeviceListWaiterTest, TriggersCallbackWhenModelReadyFires) {
  TestFuture<void> future;
  send_tab_to_self_service()->GetFakeSendTabToSelfModel()->SetIsReady(false);
  // The stub service returns this display reason regardless of whether the
  // fake model is ready, so this only verifies that OnModelReady() makes the
  // waiter re-check it.
  send_tab_to_self_service()->SetEntryPointDisplayReason(std::nullopt);

  TargetDeviceListWaiter waiter(sync_service(), send_tab_to_self_service(),
                                GURL(kTestUrl), future.GetCallback());

  EXPECT_FALSE(future.IsReady());

  send_tab_to_self_service()->SetEntryPointDisplayReason(
      EntryPointDisplayReason::kOfferFeature);
  send_tab_to_self_service()->GetFakeSendTabToSelfModel()->SetIsReady(true);

  EXPECT_TRUE(future.Wait());
}

// Verifies that a SyncService state change that arrives while the model is
// still loading from disk does not resolve the waiter, and that the waiter
// resolves once the model later notifies OnModelReady(). Unlike
// TriggersCallbackWhenModelReadyFires, this covers a Sync state change that
// happens before the model is ready.
TEST_F(TargetDeviceListWaiterTest,
       SyncStateChangeWhileModelLoadingDoesNotResolve) {
  TestFuture<void> future;
  FakeSendTabToSelfModel* model =
      send_tab_to_self_service()->GetFakeSendTabToSelfModel();
  model->SetIsReady(false);
  // The stub service returns this display reason regardless of whether the
  // fake model is ready, so std::nullopt simulates the model still loading.
  send_tab_to_self_service()->SetEntryPointDisplayReason(std::nullopt);

  TargetDeviceListWaiter waiter(sync_service(), send_tab_to_self_service(),
                                GURL(kTestUrl), future.GetCallback());

  // A Sync state change while the model is still loading must not resolve
  // the waiter.
  sync_service()->FireStateChanged();
  FlushPostedTasks();
  EXPECT_FALSE(future.IsReady());

  // The model finishes loading and the device list is now known.
  send_tab_to_self_service()->SetEntryPointDisplayReason(
      EntryPointDisplayReason::kOfferFeature);
  model->SetIsReady(true);

  EXPECT_TRUE(future.Wait());
}

// Verifies that OnModelReady() does not resolve the waiter while the display
// reason is still pending (std::nullopt or kOfferSignIn).
TEST_F(TargetDeviceListWaiterTest, ModelReadyKeepsWaitingWhileReasonPending) {
  TestFuture<void> future;
  int callback_count = 0;
  FakeSendTabToSelfModel* model =
      send_tab_to_self_service()->GetFakeSendTabToSelfModel();
  model->SetIsReady(false);
  send_tab_to_self_service()->SetEntryPointDisplayReason(std::nullopt);

  TargetDeviceListWaiter waiter(sync_service(), send_tab_to_self_service(),
                                GURL(kTestUrl),
                                base::BindLambdaForTesting([&]() {
                                  callback_count++;
                                  future.SetValue();
                                }));

  model->SetIsReady(true);
  FlushPostedTasks();
  EXPECT_FALSE(future.IsReady());

  send_tab_to_self_service()->SetEntryPointDisplayReason(
      EntryPointDisplayReason::kOfferSignIn);
  model->SetIsReady(true);
  FlushPostedTasks();
  EXPECT_FALSE(future.IsReady());

  send_tab_to_self_service()->SetEntryPointDisplayReason(
      EntryPointDisplayReason::kInformNoTargetDevice);
  model->SetIsReady(true);
  EXPECT_TRUE(future.Wait());

  // A later model ready notification must not run the callback again.
  model->SetIsReady(true);
  FlushPostedTasks();
  EXPECT_THAT(callback_count, Eq(1));
}

}  // namespace

}  // namespace send_tab_to_self
