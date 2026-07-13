// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/tab_drag/tab_drag_service_impl.h"

#include <algorithm>
#include <memory>
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
    toy_window_ = toy_window.get();
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

    toy_window_ = nullptr;
    injector_ = nullptr;

    service_.reset();
    session_manager_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  void RegisterDropTargetForService(
      TabDragServiceImpl* service,
      mojo::Remote<mojom::TabDragService>& remote,
      ToyDropTarget& target,
      mojo::AssociatedReceiver<mojom::DropTarget>& target_receiver,
      mojo::AssociatedRemote<mojom::DropTargetRegistration>& registration,
      const gfx::Rect& bounds) {
    service->Accept(remote.BindNewPipeAndPassReceiver(), gfx::NativeView());
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
    registration->OnBoundsChanged(bounds);
  }

  void StartDefaultDrag(mojo::Remote<mojom::TabDragService>& remote,
                        const std::vector<tabs_api::NodeId>& tab_ids,
                        const gfx::Point& start_point) {
    base::RunLoop drag_start_loop;
    remote->StartDrag(tab_ids, start_point,
                      base::BindLambdaForTesting(
                          [&](mojom::TabDragService::StartDragResult result) {
                            ASSERT_TRUE(result.has_value());
                            drag_start_loop.Quit();
                          }));
    drag_start_loop.Run();
  }

  bool WaitForEvent(const ToyDropTarget& target,
                    ToyDropTarget::ReceivedEvent::Type type) {
    return base::test::RunUntil([&]() {
      return std::ranges::any_of(target.events(), [type](const auto& event) {
        return event.type == type;
      });
    });
  }

  std::unique_ptr<ToyTabDragWindowAdapter> CreateAndConfigureDetachedWindow(
      ToyTabDragWindowAdapter* source_window,
      const std::vector<gfx::Point>& simulated_moves) {
    auto detached_window = std::make_unique<ToyTabDragWindowAdapter>(
        gfx::Rect(0, 0, 400, 300), injector_->GetWindowRegistry());
    source_window->set_detach_to_new_window_result(
        detached_window->GetWindowId());
    detached_window->set_simulated_moves(simulated_moves);
    return detached_window;
  }

  raw_ptr<TestTabDragSessionInjector> injector_ = nullptr;
  raw_ptr<ToyTabDragWindowAdapter> toy_window_ = nullptr;
  TabDragWindowId window_id_;
  std::unique_ptr<TabDragSessionManager> session_manager_;
  std::unique_ptr<TabDragServiceImpl> service_;

  ToyDropTarget drop_target_;
};

IN_PROC_BROWSER_TEST_F(TabDragServiceImplBrowserTest, IntraWindowDragAndDrop) {
  mojo::Remote<mojom::TabDragService> remote;
  mojo::AssociatedReceiver<mojom::DropTarget> target_receiver{&drop_target_};
  mojo::AssociatedRemote<mojom::DropTargetRegistration> registration;

  RegisterDropTargetForService(service_.get(), remote, drop_target_,
                               target_receiver, registration,
                               gfx::Rect(0, 0, 800, 600));

  tabs_api::NodeId tab_id(NodeId::Type::kContent, "1");
  StartDefaultDrag(remote, {tab_id}, gfx::Point(100, 100));
  ASSERT_TRUE(
      WaitForEvent(drop_target_, ToyDropTarget::ReceivedEvent::Type::kEntered));

  injector_->toy_input_adapter().SendToyEvent(TabDragInputEvent::Type::kMoved,
                                              gfx::Point(120, 100));
  ASSERT_TRUE(
      WaitForEvent(drop_target_, ToyDropTarget::ReceivedEvent::Type::kDrag));

  injector_->toy_input_adapter().SendToyEvent(TabDragInputEvent::Type::kDropped,
                                              gfx::Point(120, 100));
  ASSERT_TRUE(
      WaitForEvent(drop_target_, ToyDropTarget::ReceivedEvent::Type::kDrop));
  EXPECT_EQ(drop_target_.events().back().tab_ids[0], tab_id);
}

IN_PROC_BROWSER_TEST_F(TabDragServiceImplBrowserTest, InterWindowDragAndDrop) {
  mojo::Remote<mojom::TabDragService> remote_a;
  mojo::AssociatedReceiver<mojom::DropTarget> target_receiver_a{&drop_target_};
  mojo::AssociatedRemote<mojom::DropTargetRegistration> registration_a;
  RegisterDropTargetForService(service_.get(), remote_a, drop_target_,
                               target_receiver_a, registration_a,
                               gfx::Rect(0, 0, 800, 600));

  auto toy_window_b = std::make_unique<ToyTabDragWindowAdapter>(
      gfx::Rect(800, 0, 800, 600), injector_->GetWindowRegistry());
  auto service_b = std::make_unique<TabDragServiceImpl>(
      session_manager_.get(), std::move(toy_window_b));
  ToyDropTarget drop_target_b;
  mojo::Remote<mojom::TabDragService> remote_b;
  mojo::AssociatedReceiver<mojom::DropTarget> target_receiver_b{&drop_target_b};
  mojo::AssociatedRemote<mojom::DropTargetRegistration> registration_b;
  RegisterDropTargetForService(service_b.get(), remote_b, drop_target_b,
                               target_receiver_b, registration_b,
                               gfx::Rect(0, 0, 800, 600));

  auto detached_window =
      CreateAndConfigureDetachedWindow(toy_window_, {gfx::Point(900, 100)});

  StartDefaultDrag(remote_a, {NodeId(NodeId::Type::kContent, "1")},
                   gfx::Point(100, 100));
  ASSERT_TRUE(
      WaitForEvent(drop_target_, ToyDropTarget::ReceivedEvent::Type::kEntered));

  // Move into Window B area (triggers tear-off and transfer).
  injector_->toy_input_adapter().SendToyEvent(TabDragInputEvent::Type::kMoved,
                                              gfx::Point(900, 100));
  ASSERT_TRUE(
      WaitForEvent(drop_target_, ToyDropTarget::ReceivedEvent::Type::kLeave));
  ASSERT_TRUE(WaitForEvent(drop_target_b,
                           ToyDropTarget::ReceivedEvent::Type::kEntered));

  // Drop in Window B.
  injector_->toy_input_adapter().SendToyEvent(TabDragInputEvent::Type::kDropped,
                                              gfx::Point(900, 100));
  ASSERT_TRUE(
      WaitForEvent(drop_target_b, ToyDropTarget::ReceivedEvent::Type::kDrop));
}

IN_PROC_BROWSER_TEST_F(TabDragServiceImplBrowserTest, DragCancellation) {
  mojo::Remote<mojom::TabDragService> remote;
  mojo::AssociatedReceiver<mojom::DropTarget> target_receiver{&drop_target_};
  mojo::AssociatedRemote<mojom::DropTargetRegistration> registration;
  RegisterDropTargetForService(service_.get(), remote, drop_target_,
                               target_receiver, registration,
                               gfx::Rect(0, 0, 800, 600));

  StartDefaultDrag(remote, {NodeId(NodeId::Type::kContent, "1")},
                   gfx::Point(100, 100));
  ASSERT_TRUE(
      WaitForEvent(drop_target_, ToyDropTarget::ReceivedEvent::Type::kEntered));

  injector_->toy_input_adapter().SendToyEvent(TabDragInputEvent::Type::kMoved,
                                              gfx::Point(120, 100));
  ASSERT_TRUE(
      WaitForEvent(drop_target_, ToyDropTarget::ReceivedEvent::Type::kDrag));

  // Cancel drag session (e.g., ESC key pressed).
  injector_->toy_input_adapter().SendToyEvent(
      TabDragInputEvent::Type::kCancelled);
  ASSERT_TRUE(WaitForEvent(drop_target_,
                           ToyDropTarget::ReceivedEvent::Type::kCancelled));
}

IN_PROC_BROWSER_TEST_F(TabDragServiceImplBrowserTest, DragTearOff) {
  mojo::Remote<mojom::TabDragService> remote;
  mojo::AssociatedReceiver<mojom::DropTarget> target_receiver{&drop_target_};
  mojo::AssociatedRemote<mojom::DropTargetRegistration> registration;
  RegisterDropTargetForService(service_.get(), remote, drop_target_,
                               target_receiver, registration,
                               gfx::Rect(0, 0, 800, 600));

  auto detached_window =
      CreateAndConfigureDetachedWindow(toy_window_, {gfx::Point(1200, 100)});

  StartDefaultDrag(remote, {NodeId(NodeId::Type::kContent, "1")},
                   gfx::Point(100, 100));
  ASSERT_TRUE(
      WaitForEvent(drop_target_, ToyDropTarget::ReceivedEvent::Type::kEntered));

  // Move outside all windows into empty desktop space.
  injector_->toy_input_adapter().SendToyEvent(TabDragInputEvent::Type::kMoved,
                                              gfx::Point(1200, 100));
  ASSERT_TRUE(
      WaitForEvent(drop_target_, ToyDropTarget::ReceivedEvent::Type::kLeave));

  // Drop detached window.
  injector_->toy_input_adapter().SendToyEvent(TabDragInputEvent::Type::kDropped,
                                              gfx::Point(1200, 100));
}

}  // namespace
}  // namespace tabs_api
