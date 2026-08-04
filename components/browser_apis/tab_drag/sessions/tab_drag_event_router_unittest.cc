// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/tab_drag/sessions/tab_drag_event_router.h"

#include <memory>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "components/browser_apis/tab_drag/destinations/drop_target_registry.h"
#include "components/browser_apis/tab_drag/destinations/drop_target_registry_impl.h"
#include "components/browser_apis/tab_drag/sessions/tab_drag_window_registry.h"
#include "components/browser_apis/tab_drag/testing/toy_drop_target.h"
#include "components/browser_apis/tab_drag/testing/toy_tab_drag_window_adapter.h"
#include "components/browser_apis/tab_strip/types/node_id.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_ui_types.h"

namespace tabs_api {

class TabDragEventRouterTest : public ::testing::Test {
 protected:
  TabDragEventRouterTest() : router_(registry_) {}
  ~TabDragEventRouterTest() override = default;

  base::test::SingleThreadTaskEnvironment task_environment_;
  TabDragWindowRegistry window_registry_;
  DropTargetRegistryImpl registry_;
  TabDragEventRouter router_;
};

TEST_F(TabDragEventRouterTest, RouteMoveEvents) {
  ToyTabDragWindowAdapter window(gfx::Rect(10, 10, 100, 100),
                                 &window_registry_);
  ToyDropTarget target;

  mojo::AssociatedRemote<mojom::DropTarget> remote;
  mojo::AssociatedReceiver<mojom::DropTarget> bound_receiver(
      &target, remote.BindNewEndpointAndPassDedicatedReceiver());
  mojo::AssociatedRemote<mojom::DropTargetRegistration> registration;

  registry_.RegisterDropTarget(
      &window, gfx::NativeView(), remote.Unbind(),
      registration.BindNewEndpointAndPassDedicatedReceiver());

  std::vector<NodeId> tabs = {NodeId(NodeId::Type::kContent, "tab1")};
  router_.OnSessionStarted(tabs, window.GetWindowId(), gfx::Point(50, 50), 0);

  ASSERT_TRUE(
      base::test::RunUntil([&]() { return target.events().size() == 1u; }));
  EXPECT_EQ(ToyDropTarget::ReceivedEvent::Type::kEntered,
            target.events()[0].type);
  EXPECT_EQ(gfx::Point(40, 40), target.events()[0].local_point);
  EXPECT_EQ(tabs, target.events()[0].tab_ids);

  // Simulate moving inside the window
  router_.OnDragMoved(gfx::Point(60, 70));
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return target.events().size() == 2u; }));
  EXPECT_EQ(ToyDropTarget::ReceivedEvent::Type::kDrag, target.events()[1].type);
  EXPECT_EQ(gfx::Point(50, 60), target.events()[1].local_point);

  // Simulate leaving the window
  router_.OnTargetChanged(DropTargetId(), gfx::Point(5, 5));
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return target.events().size() == 3u; }));
  EXPECT_EQ(ToyDropTarget::ReceivedEvent::Type::kLeave,
            target.events()[2].type);

  router_.OnSessionCancelled();
}

TEST_F(TabDragEventRouterTest, MultiWindowRouting) {
  ToyTabDragWindowAdapter window_a(gfx::Rect(0, 0, 100, 100),
                                   &window_registry_);
  ToyDropTarget target_a;
  mojo::AssociatedRemote<mojom::DropTarget> remote_a;
  mojo::AssociatedReceiver<mojom::DropTarget> bound_receiver_a(
      &target_a, remote_a.BindNewEndpointAndPassDedicatedReceiver());
  mojo::AssociatedRemote<mojom::DropTargetRegistration> reg_a;
  registry_.RegisterDropTarget(&window_a, gfx::NativeView(), remote_a.Unbind(),
                               reg_a.BindNewEndpointAndPassDedicatedReceiver());

  ToyTabDragWindowAdapter window_b(gfx::Rect(200, 0, 100, 100),
                                   &window_registry_);
  ToyDropTarget target_b;
  mojo::AssociatedRemote<mojom::DropTarget> remote_b;
  mojo::AssociatedReceiver<mojom::DropTarget> bound_receiver_b(
      &target_b, remote_b.BindNewEndpointAndPassDedicatedReceiver());
  mojo::AssociatedRemote<mojom::DropTargetRegistration> reg_b;
  DropTargetId id_b = registry_.RegisterDropTarget(
      &window_b, gfx::NativeView(), remote_b.Unbind(),
      reg_b.BindNewEndpointAndPassDedicatedReceiver());

  router_.OnSessionStarted({}, window_a.GetWindowId(), gfx::Point(50, 50), 0);

  ASSERT_TRUE(
      base::test::RunUntil([&]() { return target_a.events().size() == 1u; }));
  EXPECT_EQ(ToyDropTarget::ReceivedEvent::Type::kEntered,
            target_a.events()[0].type);
  EXPECT_EQ(0u, target_b.events().size());

  // Transition to B
  router_.OnTargetChanged(id_b, gfx::Point(250, 50));
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return target_a.events().size() == 2u && target_b.events().size() == 1u;
  }));

  // A should get Leave
  EXPECT_EQ(ToyDropTarget::ReceivedEvent::Type::kLeave,
            target_a.events()[1].type);

  // B should get Enter
  EXPECT_EQ(ToyDropTarget::ReceivedEvent::Type::kEntered,
            target_b.events()[0].type);
  EXPECT_EQ(gfx::Point(50, 50), target_b.events()[0].local_point);

  router_.OnSessionCancelled();
}

TEST_F(TabDragEventRouterTest, DropEvent) {
  ToyTabDragWindowAdapter window(gfx::Rect(0, 0, 100, 100), &window_registry_);
  ToyDropTarget target;
  mojo::AssociatedRemote<mojom::DropTarget> remote;
  mojo::AssociatedReceiver<mojom::DropTarget> bound_receiver(
      &target, remote.BindNewEndpointAndPassDedicatedReceiver());
  mojo::AssociatedRemote<mojom::DropTargetRegistration> reg;
  registry_.RegisterDropTarget(&window, gfx::NativeView(), remote.Unbind(),
                               reg.BindNewEndpointAndPassDedicatedReceiver());

  std::vector<NodeId> tabs = {NodeId(NodeId::Type::kContent, "tab1")};
  router_.OnSessionStarted(tabs, window.GetWindowId(), gfx::Point(50, 50), 0);
  router_.OnSessionDropped(gfx::Point(60, 60));

  ASSERT_TRUE(
      base::test::RunUntil([&]() { return target.events().size() == 2u; }));
  EXPECT_EQ(ToyDropTarget::ReceivedEvent::Type::kEntered,
            target.events()[0].type);
  EXPECT_EQ(ToyDropTarget::ReceivedEvent::Type::kDrop, target.events()[1].type);
  EXPECT_EQ(gfx::Point(60, 60), target.events()[1].local_point);
  EXPECT_EQ(tabs, target.events()[1].tab_ids);
}

TEST_F(TabDragEventRouterTest, CancelEvent) {
  ToyTabDragWindowAdapter window(gfx::Rect(0, 0, 100, 100), &window_registry_);
  ToyDropTarget target;
  mojo::AssociatedRemote<mojom::DropTarget> remote;
  mojo::AssociatedReceiver<mojom::DropTarget> bound_receiver(
      &target, remote.BindNewEndpointAndPassDedicatedReceiver());
  mojo::AssociatedRemote<mojom::DropTargetRegistration> reg;
  registry_.RegisterDropTarget(&window, gfx::NativeView(), remote.Unbind(),
                               reg.BindNewEndpointAndPassDedicatedReceiver());

  router_.OnSessionStarted({}, window.GetWindowId(), gfx::Point(50, 50), 0);
  router_.OnSessionCancelled();

  ASSERT_TRUE(
      base::test::RunUntil([&]() { return target.events().size() == 2u; }));
  EXPECT_EQ(ToyDropTarget::ReceivedEvent::Type::kEntered,
            target.events()[0].type);
  EXPECT_EQ(ToyDropTarget::ReceivedEvent::Type::kCancelled,
            target.events()[1].type);
}

}  // namespace tabs_api
