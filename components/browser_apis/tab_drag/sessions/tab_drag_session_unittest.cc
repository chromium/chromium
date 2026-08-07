// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/tab_drag/sessions/tab_drag_session.h"

#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "components/browser_apis/tab_drag/adapters/tab_drag_session_input_adapter.h"
#include "components/browser_apis/tab_drag/destinations/drop_target.h"
#include "components/browser_apis/tab_drag/destinations/drop_target_registry.h"
#include "components/browser_apis/tab_drag/sessions/tab_drag_session_injector.h"
#include "components/browser_apis/tab_drag/sessions/tab_drag_session_listener.h"
#include "components/browser_apis/tab_drag/sessions/tab_drag_window_registry.h"
#include "components/browser_apis/tab_drag/testing/toy_drop_target_registry.h"
#include "components/browser_apis/tab_drag/testing/toy_tab_drag_session_input_adapter.h"
#include "components/browser_apis/tab_drag/testing/toy_tab_drag_session_listener.h"
#include "components/browser_apis/tab_drag/testing/toy_tab_drag_window_adapter.h"
#include "components/browser_apis/tab_strip/types/node_id.h"
#include "mojo/public/mojom/base/error.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/vector2d.h"

namespace tabs_api {

class TabDragSessionTest : public ::testing::Test {
 protected:
  TabDragSessionTest()
      : dummy_window_(gfx::Rect(0, 0, 100, 100), &registry_),
        dummy_detached_window_(gfx::Rect(0, 0, 100, 100), &registry_) {
    dummy_window_.set_detach_to_new_window_result(
        dummy_detached_window_.GetWindowId());
  }
  ~TabDragSessionTest() override = default;

  TabDragWindowRegistry registry_;
  ToyTabDragWindowAdapter dummy_window_;
  ToyTabDragWindowAdapter dummy_detached_window_;
};


TEST_F(TabDragSessionTest, StartAndReleaseCapture) {
  ToyTabDragSessionInputAdapter toy_adapter;
  ToyTabDragSessionListener listener;
  ToyDropTargetRegistry dummy_registry;
  TabDragWindowRegistry registry;
  ToyTabDragSessionInjector injector(toy_adapter, listener, dummy_registry,
                                     &registry);
  base::MockOnceClosure end_callback;
  ToyTabDragWindowAdapter toy_window(gfx::Rect(0, 0, 100, 100), &registry);

  EXPECT_FALSE(toy_adapter.capture_started());
  EXPECT_FALSE(toy_adapter.capture_released());
  EXPECT_FALSE(toy_window.HasCapture());

  {
    TabDragSessionParams params{
        .source_window_id = toy_window.GetWindowId(),
        .source_tab_ids = {NodeId(NodeId::Type::kContent, "tab1")},
        .start_point = gfx::Point(),
        .end_callback = end_callback.Get()};
    TabDragSession session(std::move(params), &injector);
    EXPECT_FALSE(toy_adapter.capture_started());
    EXPECT_TRUE(session.Start().has_value());
    EXPECT_TRUE(toy_adapter.capture_started());
    EXPECT_FALSE(toy_adapter.capture_released());
    EXPECT_TRUE(toy_window.HasCapture());
  }

  EXPECT_TRUE(toy_adapter.capture_released());
  EXPECT_FALSE(toy_window.HasCapture());
}

TEST_F(TabDragSessionTest, InputEventCancelled) {
  ToyTabDragSessionInputAdapter toy_adapter;
  ToyTabDragSessionListener listener;
  ToyDropTargetRegistry dummy_registry;
  ToyTabDragSessionInjector injector(toy_adapter, listener, dummy_registry,
                                     &registry_);
  base::MockOnceClosure end_callback;

  TabDragSessionParams params{
      .source_window_id = dummy_window_.GetWindowId(),
      .source_tab_ids = {NodeId(NodeId::Type::kContent, "tab1")},
      .start_point = gfx::Point(),
      .end_callback = end_callback.Get()};
  TabDragSession session(std::move(params), &injector);
  EXPECT_TRUE(session.Start().has_value());

  EXPECT_CALL(end_callback, Run()).Times(1);
  toy_adapter.SendToyEvent(TabDragInputEvent::Type::kCancelled);
}

TEST_F(TabDragSessionTest, InputEventDropped) {
  ToyTabDragSessionInputAdapter toy_adapter;
  ToyTabDragSessionListener listener;
  ToyDropTargetRegistry dummy_registry;
  ToyTabDragSessionInjector injector(toy_adapter, listener, dummy_registry,
                                     &registry_);
  base::MockOnceClosure end_callback;

  TabDragSessionParams params{
      .source_window_id = dummy_window_.GetWindowId(),
      .source_tab_ids = {NodeId(NodeId::Type::kContent, "tab1")},
      .start_point = gfx::Point(),
      .end_callback = end_callback.Get()};
  TabDragSession session(std::move(params), &injector);
  EXPECT_TRUE(session.Start().has_value());

  EXPECT_CALL(end_callback, Run()).Times(1);
  toy_adapter.SendToyEvent(TabDragInputEvent::Type::kDropped);
}

TEST_F(TabDragSessionTest, CoordinateTracking) {
  ToyTabDragSessionInputAdapter toy_adapter;
  ToyTabDragSessionListener listener;
  ToyDropTargetRegistry dummy_registry;
  dummy_registry.set_source_window(&dummy_window_);
  ToyTabDragSessionInjector injector(toy_adapter, listener, dummy_registry,
                                     &registry_);
  base::MockOnceClosure end_callback;

  gfx::Point start_point(10, 10);
  TabDragSessionParams params{
      .source_window_id = dummy_window_.GetWindowId(),
      .source_tab_ids = {NodeId(NodeId::Type::kContent, "tab1")},
      .start_point = start_point,
      .end_callback = end_callback.Get()};
  TabDragSession session(std::move(params), &injector);
  EXPECT_TRUE(session.Start().has_value());

  EXPECT_EQ(session.start_point_in_screen(), start_point);
  EXPECT_EQ(session.last_mouse_screen_point(), start_point);
  EXPECT_EQ(session.delta(), gfx::Vector2d(0, 0));

  // Move mouse
  gfx::Point move_point(15, 20);
  toy_adapter.SendToyEvent(TabDragInputEvent::Type::kMoved, move_point);

  EXPECT_EQ(session.last_mouse_screen_point(), move_point);
  EXPECT_EQ(session.delta(), gfx::Vector2d(5, 10));

  // Drop mouse
  gfx::Point drop_point(25, 30);
  EXPECT_CALL(end_callback, Run()).Times(1);
  toy_adapter.SendToyEvent(TabDragInputEvent::Type::kDropped, drop_point);

  EXPECT_EQ(session.last_mouse_screen_point(), drop_point);
  EXPECT_EQ(session.delta(), gfx::Vector2d(15, 20));
}

TEST_F(TabDragSessionTest, ListenerNotification) {
  ToyTabDragSessionInputAdapter toy_adapter;
  base::MockOnceClosure end_callback;
  ToyTabDragSessionListener listener;
  ToyDropTargetRegistry registry;
  registry.set_source_window(&dummy_window_);
  ToyTabDragSessionInjector injector(toy_adapter, listener, registry,
                                     &registry_);
  ToyTabDragWindowAdapter target_window(gfx::Rect(0, 0, 100, 100), &registry_);

  std::vector<tabs_api::NodeId> tab_ids = {
      NodeId(NodeId::Type::kContent, "tab1")};
  TabDragSessionParams params{.source_window_id = dummy_window_.GetWindowId(),
                              .source_tab_ids = tab_ids,
                              .start_point = gfx::Point(),
                              .end_callback = end_callback.Get()};
  TabDragSession session(std::move(params), &injector);

  ASSERT_EQ(listener.events().size(), 0u);
  EXPECT_TRUE(session.Start().has_value());
  ASSERT_EQ(listener.events().size(), 1u);
  EXPECT_EQ(listener.events()[0].type,
            ToyTabDragSessionListener::Event::Type::kStarted);
  EXPECT_EQ(listener.events()[0].dragged_tabs, tab_ids);
  EXPECT_EQ(listener.events()[0].window_id, dummy_window_.GetWindowId());
  EXPECT_EQ(listener.events()[0].point, gfx::Point());

  // Move outside source window to trigger tear-off.
  // This will trigger tear-off, call RunWindowMoveLoop (which returns
  // kSuccess), and immediately drop and end the session.
  EXPECT_CALL(end_callback, Run()).Times(1);
  gfx::Point tear_point(120, 120);
  toy_adapter.SendToyEvent(TabDragInputEvent::Type::kMoved, tear_point);

  // We expect 3 events: kStarted, kDetached, kDropped.
  ASSERT_EQ(listener.events().size(), 3u);
  EXPECT_EQ(listener.events()[0].type,
            ToyTabDragSessionListener::Event::Type::kStarted);
  EXPECT_EQ(listener.events()[1].type,
            ToyTabDragSessionListener::Event::Type::kDetached);
  EXPECT_EQ(listener.events()[1].point, tear_point);
  EXPECT_EQ(listener.events()[2].type,
            ToyTabDragSessionListener::Event::Type::kDropped);
  EXPECT_EQ(listener.events()[2].point, tear_point);

  // Verify that the detachment and move loop were called on the windows.
  EXPECT_TRUE(dummy_window_.detach_to_new_window_called());
  EXPECT_EQ(dummy_window_.last_detach_tab_ids(), tab_ids);
  EXPECT_EQ(dummy_window_.last_detach_drag_offset(), gfx::Vector2d(0, 0));
  EXPECT_TRUE(dummy_detached_window_.run_window_move_loop_called());
  EXPECT_FALSE(dummy_detached_window_.had_capture_on_move_loop());
  EXPECT_EQ(dummy_detached_window_.last_move_loop_point(), tear_point);
  EXPECT_EQ(dummy_detached_window_.last_move_loop_offset(),
            gfx::Vector2d(0, 0));
}

TEST_F(TabDragSessionTest, CaptureLostExternally) {
  ToyTabDragSessionInputAdapter toy_adapter;
  ToyTabDragSessionListener listener;
  ToyDropTargetRegistry registry;
  TabDragWindowRegistry window_registry;
  ToyTabDragSessionInjector injector(toy_adapter, listener, registry,
                                     &window_registry);
  base::MockOnceClosure end_callback;
  ToyTabDragWindowAdapter toy_window(gfx::Rect(0, 0, 100, 100),
                                     &window_registry);

  TabDragSessionParams params{
      .source_window_id = toy_window.GetWindowId(),
      .source_tab_ids = {NodeId(NodeId::Type::kContent, "tab1")},
      .start_point = gfx::Point(),
      .end_callback = end_callback.Get()};
  TabDragSession session(std::move(params), &injector);
  EXPECT_TRUE(session.Start().has_value());
  EXPECT_TRUE(toy_window.HasCapture());

  // Simulate external capture loss.
  toy_window.ReleaseCapture();
  EXPECT_CALL(end_callback, Run()).Times(1);
  toy_adapter.SendToyEvent(TabDragInputEvent::Type::kCaptureChanged);
}

TEST_F(TabDragSessionTest, DropTargetBoundsTearOff) {
  ToyTabDragSessionInputAdapter toy_adapter;
  ToyTabDragSessionListener listener;
  ToyDropTargetRegistry registry;
  registry.set_source_window(&dummy_window_);
  ToyTabDragSessionInjector injector(toy_adapter, listener, registry,
                                     &registry_);
  base::MockOnceClosure end_callback;

  // Set cached bounds on the source drop target.
  // Window bounds are (0, 0, 100, 100). We set drop target bounds to (10, 10,
  // 80, 20). With kTearThreshold = 15, the tear-off bounds will be (-5, -5,
  // 110, 50).
  registry.UpdateTargetBounds(registry.source_id(), gfx::Rect(10, 10, 80, 20));

  std::vector<tabs_api::NodeId> tab_ids = {
      NodeId(NodeId::Type::kContent, "tab1")};
  TabDragSessionParams params{.source_window_id = dummy_window_.GetWindowId(),
                              .source_tab_ids = tab_ids,
                              .start_point = gfx::Point(),
                              .end_callback = end_callback.Get()};
  TabDragSession session(std::move(params), &injector);

  EXPECT_TRUE(session.Start().has_value());

  // Move mouse to (50, 30). This is inside the active bounds (-5, -5, 110, 50).
  // It should remain attached.
  gfx::Point inside_point(50, 30);
  toy_adapter.SendToyEvent(TabDragInputEvent::Type::kMoved, inside_point);
  ASSERT_EQ(listener.events().size(), 2u);  // Started, DragMoved
  EXPECT_EQ(listener.events()[1].type,
            ToyTabDragSessionListener::Event::Type::kMoved);

  // Move mouse to (50, 60). This is outside the active bounds (-5, -5, 110, 50)
  // but inside the window. It should trigger tear-off, call RunWindowMoveLoop
  // (returns kSuccess), and immediately drop and end the session.
  EXPECT_CALL(end_callback, Run()).Times(1);
  gfx::Point tear_point(50, 60);
  toy_adapter.SendToyEvent(TabDragInputEvent::Type::kMoved, tear_point);

  // We expect 4 events: kStarted, kMoved (inside), kDetached, kDropped.
  ASSERT_EQ(listener.events().size(), 4u);
  EXPECT_EQ(listener.events()[2].type,
            ToyTabDragSessionListener::Event::Type::kDetached);
  EXPECT_EQ(listener.events()[2].point, tear_point);
  EXPECT_EQ(listener.events()[3].type,
            ToyTabDragSessionListener::Event::Type::kDropped);
  EXPECT_EQ(listener.events()[3].point, tear_point);
}

TEST_F(TabDragSessionTest, DropTargetBoundsTearOffCancel) {
  ToyTabDragSessionInputAdapter toy_adapter;
  ToyTabDragSessionListener listener;
  ToyDropTargetRegistry registry;
  registry.set_source_window(&dummy_window_);
  ToyTabDragSessionInjector injector(toy_adapter, listener, registry,
                                     &registry_);
  base::MockOnceClosure end_callback;

  std::vector<tabs_api::NodeId> tab_ids = {
      NodeId(NodeId::Type::kContent, "tab1")};
  TabDragSessionParams params{.source_window_id = dummy_window_.GetWindowId(),
                              .source_tab_ids = tab_ids,
                              .start_point = gfx::Point(),
                              .end_callback = end_callback.Get()};
  TabDragSession session(std::move(params), &injector);

  EXPECT_TRUE(session.Start().has_value());

  // Configure the mock loop to return kCanceled.
  dummy_detached_window_.set_run_window_move_loop_result(
      DragMoveLoopResult::kCanceled);

  // Move outside source window to trigger tear-off.
  // This will trigger tear-off, call RunWindowMoveLoop (which returns
  // kCanceled), and immediately cancel and end the session.
  EXPECT_CALL(end_callback, Run()).Times(1);
  gfx::Point tear_point(150, 150);
  toy_adapter.SendToyEvent(TabDragInputEvent::Type::kMoved, tear_point);

  // We expect 3 events: kStarted, kDetached, kCancelled.
  ASSERT_EQ(listener.events().size(), 3u);
  EXPECT_EQ(listener.events()[1].type,
            ToyTabDragSessionListener::Event::Type::kDetached);
  EXPECT_EQ(listener.events()[2].type,
            ToyTabDragSessionListener::Event::Type::kCancelled);
}

TEST_F(TabDragSessionTest, CaptureLostDuringDetachIgnored) {
  ToyTabDragSessionInputAdapter toy_adapter;
  ToyTabDragSessionListener listener;
  ToyDropTargetRegistry registry;
  TabDragWindowRegistry window_registry;
  ToyTabDragSessionInjector injector(toy_adapter, listener, registry,
                                     &window_registry);
  base::MockOnceClosure end_callback;
  ToyTabDragWindowAdapter toy_window(gfx::Rect(0, 0, 100, 100),
                                     &window_registry);

  TabDragSessionParams params{
      .source_window_id = toy_window.GetWindowId(),
      .source_tab_ids = {NodeId(NodeId::Type::kContent, "tab1")},
      .start_point = gfx::Point(),
      .end_callback = end_callback.Get()};
  TabDragSession session(std::move(params), &injector);
  EXPECT_TRUE(session.Start().has_value());
  EXPECT_TRUE(toy_window.HasCapture());

  // Force the session into the kDetaching state.
  session.set_drag_mode_for_testing(TabDragSession::DragMode::kDetaching);

  // Simulate capture loss. It should be IGNORED.
  // We expect end_callback to NOT be called (Times(0)).
  EXPECT_CALL(end_callback, Run()).Times(0);
  toy_adapter.SendToyEvent(TabDragInputEvent::Type::kCaptureChanged);

  // Verify the session is still alive by successfully dropping it.
  // This should trigger the end_callback.
  EXPECT_CALL(end_callback, Run()).Times(1);
  toy_adapter.SendToyEvent(TabDragInputEvent::Type::kDropped);
}

TEST_F(TabDragSessionTest, SingleTabDragImmediateWindowDrag) {
  ToyTabDragSessionInputAdapter toy_adapter;
  base::MockOnceClosure end_callback;
  ToyTabDragSessionListener listener;
  ToyDropTargetRegistry registry;
  registry.set_source_window(&dummy_window_);
  ToyTabDragSessionInjector injector(toy_adapter, listener, registry,
                                     &registry_);

  // Set tab count to 1 to simulate single-tab window.
  dummy_window_.set_tab_count(1);

  gfx::Point start_point(10, 10);
  std::vector<tabs_api::NodeId> tab_ids = {
      NodeId(NodeId::Type::kContent, "tab1")};
  TabDragSessionParams params{.source_window_id = dummy_window_.GetWindowId(),
                              .source_tab_ids = tab_ids,
                              .start_point = start_point,
                              .end_callback = end_callback.Get()};
  TabDragSession session(std::move(params), &injector);

  // Single tab in window -> Start() should immediately trigger window drag
  // on the source window, call RunWindowMoveLoop (returns kSuccess), and
  // immediately drop and end the session.
  EXPECT_CALL(end_callback, Run()).Times(1);
  EXPECT_TRUE(session.Start().has_value());

  // We expect 2 events: kStarted, kDropped.
  ASSERT_EQ(listener.events().size(), 2u);
  EXPECT_EQ(listener.events()[0].type,
            ToyTabDragSessionListener::Event::Type::kStarted);
  EXPECT_EQ(listener.events()[1].type,
            ToyTabDragSessionListener::Event::Type::kDropped);
  EXPECT_EQ(listener.events()[1].point, start_point);

  // Verify that DetachToNewWindow was NOT called (we bypassed it).
  EXPECT_FALSE(dummy_window_.detach_to_new_window_called());

  // Verify that RunWindowMoveLoop was called on the SOURCE window
  // (dummy_window_) and that capture was released before entering the loop.
  EXPECT_TRUE(dummy_window_.run_window_move_loop_called());
  EXPECT_FALSE(dummy_window_.had_capture_on_move_loop());
  EXPECT_EQ(dummy_window_.last_move_loop_point(), start_point);
  EXPECT_FALSE(dummy_detached_window_.run_window_move_loop_called());
}

TEST_F(TabDragSessionTest, SingleTabDragReattachesToTargetWindow) {
  ToyTabDragSessionInputAdapter toy_adapter;
  base::MockOnceClosure end_callback;
  ToyTabDragSessionListener listener;
  ToyDropTargetRegistry registry;
  TabDragWindowRegistry window_registry;
  ToyTabDragSessionInjector injector(toy_adapter, listener, registry,
                                     &window_registry);

  ToyTabDragWindowAdapter source_window(gfx::Rect(0, 0, 100, 100),
                                        &window_registry);
  source_window.set_tab_count(1);
  registry.set_source_window(&source_window);

  ToyTabDragWindowAdapter target_window(gfx::Rect(200, 0, 100, 100),
                                        &window_registry);
  registry.set_target_window(&target_window);

  // Configure source window move loop to simulate a move to (250, 50), which
  // is within target_window's bounds (200, 0, 100, 100).
  source_window.set_simulated_moves({gfx::Point(250, 50)});

  std::vector<tabs_api::NodeId> tab_ids = {
      NodeId(NodeId::Type::kContent, "tab1")};
  TabDragSessionParams params{.source_window_id = source_window.GetWindowId(),
                              .source_tab_ids = tab_ids,
                              .start_point = gfx::Point(),
                              .end_callback = end_callback.Get()};
  TabDragSession session(std::move(params), &injector);

  // Single tab in window -> Start() immediately runs the move loop on
  // source_window, which triggers simulated move to (250, 50) and reattachment.
  EXPECT_TRUE(session.Start().has_value());

  // We expect: kStarted, kTargetChanged. The session should NOT be dropped or
  // cancelled yet.
  ASSERT_EQ(listener.events().size(), 2u);
  EXPECT_EQ(listener.events()[0].type,
            ToyTabDragSessionListener::Event::Type::kStarted);
  EXPECT_EQ(listener.events()[1].type,
            ToyTabDragSessionListener::Event::Type::kTargetChanged);
  EXPECT_EQ(listener.events()[1].target, registry.target_id());
  EXPECT_EQ(listener.events()[1].point, gfx::Point(250, 50));

  // Verify that the session's dragged window transitioned to the target window
  // and was activated.
  EXPECT_EQ(session.dragged_window(), target_window.GetWindowId());
  EXPECT_TRUE(target_window.HasCapture());
  EXPECT_TRUE(target_window.activated());

  // Drop in the target window.
  EXPECT_CALL(end_callback, Run()).Times(1);
  toy_adapter.SendToyEvent(TabDragInputEvent::Type::kDropped,
                           gfx::Point(250, 50));
  ASSERT_EQ(listener.events().size(), 3u);
  EXPECT_EQ(listener.events()[2].type,
            ToyTabDragSessionListener::Event::Type::kDropped);
  EXPECT_EQ(listener.events()[2].point, gfx::Point(250, 50));
}
}  // namespace tabs_api
