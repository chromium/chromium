// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/tab_drag/sessions/tab_drag_session.h"

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/test/mock_callback.h"
#include "components/browser_apis/tab_drag/adapters/tab_drag_session_input_adapter.h"
#include "components/browser_apis/tab_drag/testing/toy_tab_drag_session_input_adapter.h"
#include "components/browser_apis/tab_strip/types/node_id.h"
#include "mojo/public/mojom/base/error.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tabs_api {

class TabDragSessionTest : public ::testing::Test {
 protected:
  TabDragSessionTest() = default;
  ~TabDragSessionTest() override = default;
};

TEST_F(TabDragSessionTest, StartAndReleaseCapture) {
  ToyTabDragSessionInputAdapter toy_adapter;
  base::MockOnceClosure end_callback;

  EXPECT_FALSE(toy_adapter.capture_started());
  EXPECT_FALSE(toy_adapter.capture_released());

  {
    TabDragSession session({NodeId(NodeId::Type::kContent, "tab1")},
                           toy_adapter, end_callback.Get());
    EXPECT_FALSE(toy_adapter.capture_started());
    EXPECT_TRUE(session.Start().has_value());
    EXPECT_TRUE(toy_adapter.capture_started());
    EXPECT_FALSE(toy_adapter.capture_released());
  }

  EXPECT_TRUE(toy_adapter.capture_released());
}

TEST_F(TabDragSessionTest, CancelSession) {
  ToyTabDragSessionInputAdapter toy_adapter;
  base::MockOnceClosure end_callback;

  TabDragSession session({NodeId(NodeId::Type::kContent, "tab1")}, toy_adapter,
                         end_callback.Get());
  EXPECT_TRUE(session.Start().has_value());

  EXPECT_CALL(end_callback, Run()).Times(1);
  session.Cancel();
}

TEST_F(TabDragSessionTest, InputEventCancelled) {
  ToyTabDragSessionInputAdapter toy_adapter;
  base::MockOnceClosure end_callback;

  TabDragSession session({NodeId(NodeId::Type::kContent, "tab1")}, toy_adapter,
                         end_callback.Get());
  EXPECT_TRUE(session.Start().has_value());

  EXPECT_CALL(end_callback, Run()).Times(1);
  toy_adapter.SendToyEvent(TabDragInputEvent::Type::kCancelled);
}

TEST_F(TabDragSessionTest, InputEventDropped) {
  ToyTabDragSessionInputAdapter toy_adapter;
  base::MockOnceClosure end_callback;

  TabDragSession session({NodeId(NodeId::Type::kContent, "tab1")}, toy_adapter,
                         end_callback.Get());
  EXPECT_TRUE(session.Start().has_value());

  EXPECT_CALL(end_callback, Run()).Times(1);
  toy_adapter.SendToyEvent(TabDragInputEvent::Type::kDropped);
}

}  // namespace tabs_api
