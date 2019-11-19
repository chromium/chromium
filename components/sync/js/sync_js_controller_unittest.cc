// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/js/sync_js_controller.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/sync/js/js_event_details.h"
#include "components/sync/js/js_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

using ::testing::_;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::StrictMock;

class SyncJsControllerTest : public testing::Test {
 protected:
  void PumpLoop() { base::RunLoop().RunUntilIdle(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(SyncJsControllerTest, Events) {
  InSequence dummy;
  SyncJsController sync_js_controller;

  base::DictionaryValue details_dict1, details_dict2;
  details_dict1.SetString("foo", "bar");
  details_dict2.SetInteger("baz", 5);
  JsEventDetails details1(&details_dict1), details2(&details_dict2);

  StrictMock<MockJsEventHandler> event_handler1, event_handler2;
  EXPECT_CALL(event_handler1, HandleJsEvent("event", HasDetails(details1)));
  EXPECT_CALL(event_handler2, HandleJsEvent("event", HasDetails(details1)));
  EXPECT_CALL(event_handler1,
              HandleJsEvent("anotherevent", HasDetails(details2)));
  EXPECT_CALL(event_handler2,
              HandleJsEvent("anotherevent", HasDetails(details2)));

  sync_js_controller.AddJsEventHandler(&event_handler1);
  sync_js_controller.AddJsEventHandler(&event_handler2);
  sync_js_controller.HandleJsEvent("event", details1);
  sync_js_controller.HandleJsEvent("anotherevent", details2);
  sync_js_controller.RemoveJsEventHandler(&event_handler1);
  sync_js_controller.RemoveJsEventHandler(&event_handler2);
  sync_js_controller.HandleJsEvent("droppedevent", details2);

  PumpLoop();
}

}  // namespace
}  // namespace syncer
