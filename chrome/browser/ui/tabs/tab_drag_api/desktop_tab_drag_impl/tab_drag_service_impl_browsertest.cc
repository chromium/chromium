// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/tab_drag/tab_drag_service_impl.h"

#include <algorithm>
#include <ranges>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/browser_apis/tab_drag/destinations/drop_target_registry_impl.h"
#include "components/browser_apis/tab_drag/sessions/tab_drag_event_router.h"
#include "components/browser_apis/tab_drag/sessions/tab_drag_session_injector.h"
#include "components/browser_apis/tab_drag/sessions/tab_drag_session_manager.h"
#include "components/browser_apis/tab_drag/sessions/tab_drag_window_registry.h"
#include "components/browser_apis/tab_drag/tab_drag_api.mojom.h"
#include "components/browser_apis/tab_drag/testing/toy_drop_target.h"
#include "components/browser_apis/tab_drag/testing/toy_tab_drag_session_input_adapter.h"
#include "components/browser_apis/tab_drag/testing/toy_tab_drag_window_adapter.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace tabs_api {
namespace {

class TestTabDragSessionInjector : public TabDragSessionInjector {
 public:
  TestTabDragSessionInjector() : event_router_(drop_target_registry_) {}
  ~TestTabDragSessionInjector() override = default;

  TabDragWindowRegistry* GetWindowRegistry() override {
    return &window_registry_;
  }
  TabDragSessionInputAdapter& GetInputAdapter() override {
    return input_adapter_;
  }
  TabDragSessionListener& GetSessionListener() override {
    return event_router_;
  }
  DropTargetRegistry& GetDropTargetRegistry() override {
    return drop_target_registry_;
  }

  ToyTabDragSessionInputAdapter& toy_input_adapter() { return input_adapter_; }
  DropTargetRegistryImpl& drop_target_registry() {
    return drop_target_registry_;
  }

 private:
  TabDragWindowRegistry window_registry_;
  ToyTabDragSessionInputAdapter input_adapter_;
  DropTargetRegistryImpl drop_target_registry_;
  TabDragEventRouter event_router_;
};

class TabDragServiceImplBrowserTest : public InProcessBrowserTest {
 public:
  TabDragServiceImplBrowserTest() = default;
  ~TabDragServiceImplBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    auto injector = std::make_unique<TestTabDragSessionInjector>();
    injector_ = injector.get();
    session_manager_ =
        std::make_unique<TabDragSessionManager>(std::move(injector));
    auto toy_window = std::make_unique<ToyTabDragWindowAdapter>(
        gfx::Rect(0, 0, 800, 600), injector_->GetWindowRegistry());
    window_id_ = toy_window->GetWindowId();

    service_ = std::make_unique<TabDragServiceImpl>(session_manager_.get(),
                                                    std::move(toy_window));
  }

  void TearDownOnMainThread() override {
    if (window_id_ && injector_) {
      EXPECT_TRUE(base::test::RunUntil([&]() {
        return !injector_->drop_target_registry().FindTargetForWindow(
            window_id_);
      }));
    }

    injector_ = nullptr;

    service_.reset();
    session_manager_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  raw_ptr<TestTabDragSessionInjector> injector_ = nullptr;
  TabDragWindowId window_id_;
  std::unique_ptr<TabDragSessionManager> session_manager_;
  std::unique_ptr<TabDragServiceImpl> service_;

  ToyDropTarget drop_target_;
};

// Tests basic intra-window drag and drop flow using fake input.
IN_PROC_BROWSER_TEST_F(TabDragServiceImplBrowserTest, BasicDragAndDrop) {
  mojo::Remote<mojom::TabDragService> remote;
  mojo::AssociatedReceiver<mojom::DropTarget> target_receiver{&drop_target_};
  mojo::AssociatedRemote<mojom::DropTargetRegistration> registration;

  // Setup mojo.
  service_->Accept(remote.BindNewPipeAndPassReceiver(), gfx::NativeView());
  base::RunLoop register_loop;
  remote->RegisterDropTarget(
      target_receiver.BindNewEndpointAndPassRemote(),
      registration.BindNewEndpointAndPassReceiver(),
      base::BindLambdaForTesting(
          [&](mojom::TabDragService::RegisterDropTargetResult result) {
            ASSERT_TRUE(result.has_value());
            register_loop.Quit();
          }));
  register_loop.Run();
  registration->OnBoundsChanged(gfx::Rect(0, 0, 800, 600));
  EXPECT_TRUE(window_id_);
  DropTargetId target_id =
      injector_->drop_target_registry().FindTargetForWindow(window_id_);
  EXPECT_TRUE(target_id);

  // Start Drag.
  tabs_api::NodeId tab_node_id(NodeId::Type::kContent, "1");
  gfx::Point start_point(100, 100);

  base::RunLoop drag_start_loop;
  remote->StartDrag({tab_node_id}, start_point,
                    base::BindLambdaForTesting(
                        [&](mojom::TabDragService::StartDragResult result) {
                          ASSERT_TRUE(result.has_value());
                          drag_start_loop.Quit();
                        }));
  drag_start_loop.Run();

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return std::ranges::any_of(drop_target_.events(), [](const auto& event) {
      return event.type == ToyDropTarget::ReceivedEvent::Type::kEntered;
    });
  }));
  ASSERT_FALSE(drop_target_.events().empty());
  {
    auto event = std::ranges::find_if(drop_target_.events(), [](const auto& e) {
      return e.type == ToyDropTarget::ReceivedEvent::Type::kEntered;
    });
    ASSERT_NE(event, drop_target_.events().end());
    EXPECT_EQ(event->tab_ids.size(), 1u);
    EXPECT_EQ(event->tab_ids[0], tab_node_id);
    EXPECT_EQ(event->local_point, start_point);
  }

  // Simulate Drag Move.
  gfx::Point move_point(120, 100);
  injector_->toy_input_adapter().SendToyEvent(TabDragInputEvent::Type::kMoved,
                                              move_point);

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return std::ranges::any_of(drop_target_.events(), [](const auto& event) {
      return event.type == ToyDropTarget::ReceivedEvent::Type::kDrag;
    });
  }));
  {
    auto event = std::ranges::find_if(drop_target_.events(), [](const auto& e) {
      return e.type == ToyDropTarget::ReceivedEvent::Type::kDrag;
    });
    ASSERT_NE(event, drop_target_.events().end());
    EXPECT_EQ(event->local_point, move_point);
  }

  // Simulate Drop.
  injector_->toy_input_adapter().SendToyEvent(TabDragInputEvent::Type::kDropped,
                                              move_point);

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return std::ranges::any_of(drop_target_.events(), [](const auto& event) {
      return event.type == ToyDropTarget::ReceivedEvent::Type::kDrop;
    });
  }));
  {
    auto event = std::ranges::find_if(drop_target_.events(), [](const auto& e) {
      return e.type == ToyDropTarget::ReceivedEvent::Type::kDrop;
    });
    ASSERT_NE(event, drop_target_.events().end());
    EXPECT_EQ(event->tab_ids.size(), 1u);
    EXPECT_EQ(event->tab_ids[0], tab_node_id);
    EXPECT_EQ(event->local_point, move_point);
  }
}

}  // namespace
}  // namespace tabs_api
