// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/js_sync_manager_observer.h"

#include <vector>

#include "base/location.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/sync/base/model_type.h"
#include "components/sync/engine/connection_status.h"
#include "components/sync/engine/cycle/sync_cycle_snapshot.h"
#include "components/sync/engine/sync_string_conversions.h"
#include "components/sync/js/js_event_details.h"
#include "components/sync/js/js_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

using ::testing::InSequence;
using ::testing::StrictMock;

class JsSyncManagerObserverTest : public testing::Test {
 protected:
  JsSyncManagerObserverTest() {
    js_sync_manager_observer_.SetJsEventHandler(
        mock_js_event_handler_.AsWeakHandle());
  }

 private:
  // This must be destroyed after the member variables below in order
  // for WeakHandles to be destroyed properly.
  base::test::SingleThreadTaskEnvironment task_environment_;

 protected:
  StrictMock<MockJsEventHandler> mock_js_event_handler_;
  JsSyncManagerObserver js_sync_manager_observer_;

  void PumpLoop() { base::RunLoop().RunUntilIdle(); }
};

TEST_F(JsSyncManagerObserverTest, OnInitializationComplete) {
  base::DictionaryValue expected_details;
  EXPECT_CALL(mock_js_event_handler_,
              HandleJsEvent("onInitializationComplete",
                            HasDetailsAsDictionary(expected_details)));

  js_sync_manager_observer_.OnInitializationComplete(
      WeakHandle<JsBackend>(), WeakHandle<DataTypeDebugInfoListener>(), true);
  PumpLoop();
}

TEST_F(JsSyncManagerObserverTest, OnSyncCycleCompleted) {
  SyncCycleSnapshot snapshot(
      /*birthday=*/std::string(), /*bag_of_chips=*/std::string(),
      ModelNeutralState(), ProgressMarkerMap(), false, 5, 2, 7, false, 0,
      base::Time::Now(), base::Time::Now(),
      std::vector<int>(ModelType::NUM_ENTRIES, 0),
      std::vector<int>(ModelType::NUM_ENTRIES, 0),
      sync_pb::SyncEnums::UNKNOWN_ORIGIN,
      /*poll_interval=*/base::TimeDelta::FromMinutes(30),
      /*has_remaining_local_changes=*/false);
  base::DictionaryValue expected_details;
  expected_details.Set("snapshot", snapshot.ToValue());

  EXPECT_CALL(mock_js_event_handler_,
              HandleJsEvent("onSyncCycleCompleted",
                            HasDetailsAsDictionary(expected_details)));

  js_sync_manager_observer_.OnSyncCycleCompleted(snapshot);
  PumpLoop();
}

TEST_F(JsSyncManagerObserverTest, OnActionableError) {
  SyncProtocolError sync_error;
  sync_error.action = RESET_LOCAL_SYNC_DATA;
  sync_error.error_type = TRANSIENT_ERROR;
  base::DictionaryValue expected_details;
  expected_details.Set("syncError", sync_error.ToValue());

  EXPECT_CALL(mock_js_event_handler_,
              HandleJsEvent("onActionableError",
                            HasDetailsAsDictionary(expected_details)));

  js_sync_manager_observer_.OnActionableError(sync_error);
  PumpLoop();
}

TEST_F(JsSyncManagerObserverTest, OnConnectionStatusChange) {
  const ConnectionStatus kStatus = CONNECTION_AUTH_ERROR;
  base::DictionaryValue expected_details;
  expected_details.SetString("status", ConnectionStatusToString(kStatus));

  EXPECT_CALL(mock_js_event_handler_,
              HandleJsEvent("onConnectionStatusChange",
                            HasDetailsAsDictionary(expected_details)));

  js_sync_manager_observer_.OnConnectionStatusChange(kStatus);
  PumpLoop();
}

}  // namespace
}  // namespace syncer
