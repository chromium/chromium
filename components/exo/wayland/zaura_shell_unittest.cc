// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/zaura_shell.h"

#include <aura-shell-server-protocol.h>

#include <sys/socket.h>
#include <memory>

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/window_util.h"
#include "base/time/time.h"
#include "components/exo/buffer.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/wayland/scoped_wl.h"
#include "components/exo/wayland/wayland_display_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window_occlusion_tracker.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/compositor/test/layer_animator_test_controller.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_change_observer.h"
#include "ui/wm/public/activation_client.h"

namespace exo {
namespace wayland {

namespace {

constexpr auto kTransitionDuration = base::Seconds(3);

class TestAuraSurface : public AuraSurface {
 public:
  explicit TestAuraSurface(Surface* surface)
      : AuraSurface(surface, /*resource=*/nullptr) {}

  TestAuraSurface(const TestAuraSurface&) = delete;
  TestAuraSurface& operator=(const TestAuraSurface&) = delete;

  float last_sent_occlusion_fraction() const {
    return last_sent_occlusion_fraction_;
  }
  aura::Window::OcclusionState last_sent_occlusion_state() const {
    return last_sent_occlusion_state_;
  }
  int num_occlusion_updates() const { return num_occlusion_updates_; }

 protected:
  void SendOcclusionFraction(float occlusion_fraction) override {
    last_sent_occlusion_fraction_ = occlusion_fraction;
    num_occlusion_updates_++;
  }

  void SendOcclusionState(
      const aura::Window::OcclusionState occlusion_state) override {
    last_sent_occlusion_state_ = occlusion_state;
  }

 private:
  float last_sent_occlusion_fraction_ = -1.0f;
  aura::Window::OcclusionState last_sent_occlusion_state_ =
      aura::Window::OcclusionState::UNKNOWN;
  int num_occlusion_updates_ = 0;
};

class MockSurfaceDelegate : public SurfaceDelegate {
 public:
  MOCK_METHOD(void, OnSurfaceCommit, (), (override));
  MOCK_METHOD(bool, IsSurfaceSynchronized, (), (const, override));
  MOCK_METHOD(bool, IsInputEnabled, (Surface * surface), (const, override));
  MOCK_METHOD(void, OnSetFrame, (SurfaceFrameType type), (override));
  MOCK_METHOD(void,
              OnSetFrameColors,
              (SkColor active_color, SkColor inactive_color),
              (override));
  MOCK_METHOD(void,
              OnSetParent,
              (Surface * parent, const gfx::Point& position),
              (override));
  MOCK_METHOD(void, OnSetStartupId, (const char* startup_id), (override));
  MOCK_METHOD(void,
              OnSetApplicationId,
              (const char* application_id),
              (override));
  MOCK_METHOD(void, SetUseImmersiveForFullscreen, (bool value), (override));
  MOCK_METHOD(void, OnActivationRequested, (), (override));
  MOCK_METHOD(void, OnNewOutputAdded, (), (override));
  MOCK_METHOD(void, OnSetServerStartResize, (), (override));
  MOCK_METHOD(void, ShowSnapPreviewToPrimary, (), (override));
  MOCK_METHOD(void, ShowSnapPreviewToSecondary, (), (override));
  MOCK_METHOD(void, HideSnapPreview, (), (override));
  MOCK_METHOD(void, SetSnappedToSecondary, (), (override));
  MOCK_METHOD(void, SetSnappedToPrimary, (), (override));
  MOCK_METHOD(void, UnsetSnap, (), (override));
  MOCK_METHOD(void, SetCanGoBack, (), (override));
  MOCK_METHOD(void, UnsetCanGoBack, (), (override));
  MOCK_METHOD(void, SetPip, (), (override));
  MOCK_METHOD(void, UnsetPip, (), (override));
  MOCK_METHOD(void,
              SetAspectRatio,
              (const gfx::SizeF& aspect_ratio),
              (override));
  MOCK_METHOD(void, MoveToDesk, (int desk_index), (override));
  MOCK_METHOD(void, SetVisibleOnAllWorkspaces, (), (override));
  MOCK_METHOD(void,
              SetInitialWorkspace,
              (const char* initial_workspace),
              (override));
  MOCK_METHOD(void, Pin, (bool trusted), (override));
  MOCK_METHOD(void, Unpin, (), (override));
  MOCK_METHOD(void, SetSystemModal, (bool modal), (override));
  MOCK_METHOD(Capabilities*, GetCapabilities, (), (override));
};

}  // namespace

class ZAuraSurfaceTest : public test::ExoTestBase,
                         public ::wm::ActivationChangeObserver {
 public:
  ZAuraSurfaceTest() {}

  ZAuraSurfaceTest(const ZAuraSurfaceTest&) = delete;
  ZAuraSurfaceTest& operator=(const ZAuraSurfaceTest&) = delete;

  ~ZAuraSurfaceTest() override {}

  // test::ExoTestBase overrides:
  void SetUp() override {
    test::ExoTestBase::SetUp();

    gfx::Size buffer_size(10, 10);
    std::unique_ptr<Buffer> buffer(
        new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));

    surface_ = std::make_unique<Surface>();
    surface_->Attach(buffer.get());

    aura_surface_ = std::make_unique<TestAuraSurface>(surface_.get());

    gfx::Transform transform;
    transform.Scale(1.5f, 1.5f);
    parent_widget_ = CreateTestWidget();
    parent_widget_->SetBounds(gfx::Rect(0, 0, 10, 10));
    parent_widget_->GetNativeWindow()->SetTransform(transform);
    parent_widget_->GetNativeWindow()->AddChild(surface_->window());
    parent_widget_->Show();
    surface_->window()->SetBounds(gfx::Rect(5, 5, 10, 10));
    surface_->window()->Show();

    ash::Shell::Get()->activation_client()->AddObserver(this);
    aura_surface_->SetOcclusionTracking(true);
  }

  void TearDown() override {
    ash::Shell::Get()->activation_client()->RemoveObserver(this);

    parent_widget_.reset();
    aura_surface_.reset();
    surface_.reset();

    test::ExoTestBase::TearDown();
  }

  // ::wm::ActivationChangeObserver overrides:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override {
    if (lost_active == parent_widget_->GetNativeWindow()) {
      occlusion_fraction_on_activation_loss_ =
          aura_surface().last_sent_occlusion_fraction();
    }
  }

 protected:
  TestAuraSurface& aura_surface() { return *aura_surface_; }
  Surface& surface() { return *surface_; }
  views::Widget& parent_widget() { return *parent_widget_; }
  float occlusion_fraction_on_activation_loss() const {
    return occlusion_fraction_on_activation_loss_;
  }

  std::unique_ptr<views::Widget> CreateOpaqueWidget(const gfx::Rect& bounds) {
    return CreateTestWidget(
        /*delegate=*/nullptr,
        /*container_id=*/ash::desks_util::GetActiveDeskContainerId(), bounds,
        /*show=*/false);
  }

 private:
  std::unique_ptr<TestAuraSurface> aura_surface_;
  std::unique_ptr<Surface> surface_;
  std::unique_ptr<views::Widget> parent_widget_;
  float occlusion_fraction_on_activation_loss_ = -1.0f;
};

TEST_F(ZAuraSurfaceTest, OcclusionTrackingStartsAfterCommit) {
  surface().OnWindowOcclusionChanged();

  EXPECT_EQ(-1.0f, aura_surface().last_sent_occlusion_fraction());
  EXPECT_EQ(aura::Window::OcclusionState::UNKNOWN,
            aura_surface().last_sent_occlusion_state());
  EXPECT_EQ(0, aura_surface().num_occlusion_updates());
  EXPECT_FALSE(surface().IsTrackingOcclusion());

  auto widget = CreateOpaqueWidget(gfx::Rect(0, 0, 10, 10));
  widget->Show();
  surface().Commit();

  EXPECT_EQ(0.2f, aura_surface().last_sent_occlusion_fraction());
  EXPECT_EQ(aura::Window::OcclusionState::VISIBLE,
            aura_surface().last_sent_occlusion_state());
  EXPECT_EQ(1, aura_surface().num_occlusion_updates());
  EXPECT_TRUE(surface().IsTrackingOcclusion());
}

TEST_F(ZAuraSurfaceTest,
       LosingActivationWithNoAnimatingWindowsSendsCorrectOcclusionFraction) {
  surface().Commit();
  EXPECT_EQ(0.0f, aura_surface().last_sent_occlusion_fraction());
  EXPECT_EQ(aura::Window::OcclusionState::VISIBLE,
            aura_surface().last_sent_occlusion_state());
  EXPECT_EQ(1, aura_surface().num_occlusion_updates());
  ::wm::ActivateWindow(parent_widget().GetNativeWindow());

  // Creating an opaque window but don't show it.
  auto widget = CreateOpaqueWidget(gfx::Rect(0, 0, 10, 10));

  // Occlusion sent before de-activation should include that widget.
  widget->Show();
  EXPECT_EQ(0.2f, occlusion_fraction_on_activation_loss());
  EXPECT_EQ(0.2f, aura_surface().last_sent_occlusion_fraction());
  EXPECT_EQ(aura::Window::OcclusionState::VISIBLE,
            aura_surface().last_sent_occlusion_state());
  EXPECT_EQ(2, aura_surface().num_occlusion_updates());
}

TEST_F(ZAuraSurfaceTest,
       LosingActivationWithAnimatingWindowsSendsTargetOcclusionFraction) {
  surface().Commit();
  EXPECT_EQ(0.0f, aura_surface().last_sent_occlusion_fraction());
  EXPECT_EQ(aura::Window::OcclusionState::VISIBLE,
            aura_surface().last_sent_occlusion_state());
  EXPECT_EQ(1, aura_surface().num_occlusion_updates());
  ::wm::ActivateWindow(parent_widget().GetNativeWindow());

  // Creating an opaque window but don't show it.
  auto widget = CreateOpaqueWidget(gfx::Rect(0, 0, 10, 10));
  widget->GetNativeWindow()->layer()->SetOpacity(0.0f);

  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  ui::LayerAnimatorTestController test_controller(
      ui::LayerAnimator::CreateImplicitAnimator());
  ui::ScopedLayerAnimationSettings layer_animation_settings(
      test_controller.animator());
  layer_animation_settings.SetTransitionDuration(kTransitionDuration);
  widget->GetNativeWindow()->layer()->SetAnimator(test_controller.animator());
  widget->GetNativeWindow()->layer()->SetOpacity(1.0f);

  // Opacity animation uses threaded animation.
  test_controller.StartThreadedAnimationsIfNeeded();
  test_controller.Step(kTransitionDuration / 3);

  // No occlusion updates should happen until the window is de-activated.
  EXPECT_EQ(1, aura_surface().num_occlusion_updates());

  // Occlusion sent before de-activation should include the window animating
  // to be completely opaque.
  widget->Show();
  EXPECT_EQ(0.2f, occlusion_fraction_on_activation_loss());
  EXPECT_EQ(0.2f, aura_surface().last_sent_occlusion_fraction());
  EXPECT_EQ(aura::Window::OcclusionState::VISIBLE,
            aura_surface().last_sent_occlusion_state());
  EXPECT_EQ(2, aura_surface().num_occlusion_updates());

  // Explicitly stop animation because threaded animation may have started
  // a bit later. |kTransitionDuration| may not be quite enough to reach the
  // end.
  test_controller.Step(kTransitionDuration / 3);
  test_controller.Step(kTransitionDuration / 3);
  widget->GetNativeWindow()->layer()->GetAnimator()->StopAnimating();
  widget->GetNativeWindow()->layer()->SetAnimator(nullptr);

  // Expect the occlusion tracker to send an update after the animation
  // finishes.
  EXPECT_EQ(0.2f, aura_surface().last_sent_occlusion_fraction());
  EXPECT_EQ(aura::Window::OcclusionState::VISIBLE,
            aura_surface().last_sent_occlusion_state());
  EXPECT_EQ(3, aura_surface().num_occlusion_updates());
}

TEST_F(ZAuraSurfaceTest,
       LosingActivationByTriggeringTheLockScreenDoesNotSendOccludedFraction) {
  surface().Commit();
  EXPECT_EQ(0.0f, aura_surface().last_sent_occlusion_fraction());
  EXPECT_EQ(aura::Window::OcclusionState::VISIBLE,
            aura_surface().last_sent_occlusion_state());
  EXPECT_EQ(1, aura_surface().num_occlusion_updates());
  ::wm::ActivateWindow(parent_widget().GetNativeWindow());

  // Lock the screen.
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  auto lock_widget = std::make_unique<views::Widget>();
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.context = GetContext();
  params.bounds = gfx::Rect(0, 0, 100, 100);
  lock_widget->Init(std::move(params));
  ash::Shell::GetContainer(ash::Shell::GetPrimaryRootWindow(),
                           ash::kShellWindowId_LockScreenContainer)
      ->AddChild(lock_widget->GetNativeView());

  // Simulate real screen locker to change session state to LOCKED
  // when it is shown.
  auto* controller = ash::Shell::Get()->session_controller();
  GetSessionControllerClient()->LockScreen();
  lock_widget->Show();
  EXPECT_TRUE(controller->IsScreenLocked());
  EXPECT_TRUE(lock_widget->GetNativeView()->HasFocus());

  // We should have lost focus, but not reported that the window has been
  // fully occluded.
  EXPECT_NE(parent_widget().GetNativeWindow(),
            ash::window_util::GetActiveWindow());
  EXPECT_EQ(0.0f, occlusion_fraction_on_activation_loss());
  EXPECT_EQ(0.0f, aura_surface().last_sent_occlusion_fraction());
  EXPECT_EQ(aura::Window::OcclusionState::VISIBLE,
            aura_surface().last_sent_occlusion_state());
}

TEST_F(ZAuraSurfaceTest, OcclusionIncludesOffScreenArea) {
  UpdateDisplay("200x150");

  gfx::Size buffer_size(80, 100);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  // This is scaled by 1.5 - set the bounds to (-60, 75, 120, 150) in screen
  // coordinates so 75% of it is outside of the 100x100 screen.
  surface().window()->SetBounds(gfx::Rect(-40, 50, 80, 100));
  surface().Attach(buffer.get());
  surface().Commit();

  surface().OnWindowOcclusionChanged();

  EXPECT_EQ(0.75f, aura_surface().last_sent_occlusion_fraction());
  EXPECT_EQ(aura::Window::OcclusionState::VISIBLE,
            aura_surface().last_sent_occlusion_state());
}

TEST_F(ZAuraSurfaceTest, ZeroSizeWindowSendsZeroOcclusionFraction) {
  // Zero sized window should not be occluded.
  surface().window()->SetBounds(gfx::Rect(0, 0, 0, 0));
  surface().Commit();
  surface().OnWindowOcclusionChanged();
  EXPECT_EQ(0.0f, aura_surface().last_sent_occlusion_fraction());
  EXPECT_EQ(aura::Window::OcclusionState::VISIBLE,
            aura_surface().last_sent_occlusion_state());
}

TEST_F(ZAuraSurfaceTest, CanSetFullscreenModeToPlain) {
  MockSurfaceDelegate delegate;
  wl_resource resource;
  resource.data = &aura_surface();
  surface().SetSurfaceDelegate(&delegate);
  EXPECT_CALL(delegate, SetUseImmersiveForFullscreen(false));

  aura_surface().SetFullscreenMode(ZAURA_SURFACE_FULLSCREEN_MODE_PLAIN);
}

TEST_F(ZAuraSurfaceTest, CanPin) {
  MockSurfaceDelegate delegate;
  wl_resource resource;
  resource.data = &aura_surface();
  surface().SetSurfaceDelegate(&delegate);
  EXPECT_CALL(delegate, Pin(true));

  aura_surface().Pin(true);
}

TEST_F(ZAuraSurfaceTest, CanUnpin) {
  MockSurfaceDelegate delegate;
  wl_resource resource;
  resource.data = &aura_surface();
  surface().SetSurfaceDelegate(&delegate);
  EXPECT_CALL(delegate, Unpin());

  aura_surface().Unpin();
}

TEST_F(ZAuraSurfaceTest, CanSetFullscreenModeToImmersive) {
  MockSurfaceDelegate delegate;
  surface().SetSurfaceDelegate(&delegate);
  EXPECT_CALL(delegate, SetUseImmersiveForFullscreen(true));

  aura_surface().SetFullscreenMode(ZAURA_SURFACE_FULLSCREEN_MODE_IMMERSIVE);
}

class MockAuraOutput : public AuraOutput {
 public:
  using AuraOutput::AuraOutput;

  MOCK_METHOD(void, SendInsets, (const gfx::Insets&), (override));
  MOCK_METHOD(void, SendLogicalTransform, (int32_t), (override));
};

class ZAuraOutputTest : public test::ExoTestBase {
 protected:
  ZAuraOutputTest() = default;
  ZAuraOutputTest(const ZAuraOutputTest&) = delete;
  ZAuraOutputTest& operator=(const ZAuraOutputTest&) = delete;
  // test::ExxoTestBase:
  ~ZAuraOutputTest() override = default;

  void SetUp() override {
    test::ExoTestBase::SetUp();

    int fds[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fds), 0);
    wayland_display_.reset(wl_display_create());
    client_ = wl_client_create(wayland_display_.get(), fds[0]);
  }

  std::unique_ptr<MockAuraOutput> CreateAuraOutput(int version) {
    return std::make_unique<::testing::NiceMock<MockAuraOutput>>(
        wl_resource_create(client_, &zaura_output_interface, version, 0));
  }

  std::unique_ptr<wl_display, WlDisplayDeleter> wayland_display_;
  wl_client* client_ = nullptr;
};

TEST_F(ZAuraOutputTest, SendInsets) {
  auto mock_aura_output = CreateAuraOutput(ZAURA_OUTPUT_INSETS_SINCE_VERSION);

  UpdateDisplay("800x600");
  display::Display display =
      display_manager()->GetDisplayForId(display_manager()->first_display_id());
  const gfx::Rect initial_bounds{800, 600};
  EXPECT_EQ(display.bounds(), initial_bounds);
  const gfx::Rect new_work_area{10, 20, 500, 400};
  EXPECT_NE(display.work_area(), new_work_area);
  display.set_work_area(new_work_area);

  const gfx::Insets expected_insets = initial_bounds.InsetsFrom(new_work_area);
  EXPECT_CALL(*mock_aura_output, SendInsets(expected_insets)).Times(1);
  mock_aura_output->SendDisplayMetrics(
      display, display::DisplayObserver::DISPLAY_METRIC_WORK_AREA);
}

TEST_F(ZAuraOutputTest, SendLogicalTransform) {
  auto mock_aura_output =
      CreateAuraOutput(ZAURA_OUTPUT_LOGICAL_TRANSFORM_SINCE_VERSION);

  UpdateDisplay("800x600");
  display::Display display =
      display_manager()->GetDisplayForId(display_manager()->first_display_id());

  // Make sure the expected calls happen in order.
  ::testing::InSequence seq;

  EXPECT_EQ(display.rotation(), display::Display::ROTATE_0);
  EXPECT_EQ(display.panel_rotation(), display::Display::ROTATE_0);
  EXPECT_CALL(*mock_aura_output,
              SendLogicalTransform(OutputTransform(display.rotation())))
      .Times(1);
  mock_aura_output->SendDisplayMetrics(
      display, display::DisplayObserver::DISPLAY_METRIC_ROTATION);

  display.set_rotation(display::Display::ROTATE_270);
  display.set_panel_rotation(display::Display::ROTATE_180);
  EXPECT_CALL(*mock_aura_output,
              SendLogicalTransform(OutputTransform(display.rotation())))
      .Times(1);
  mock_aura_output->SendDisplayMetrics(
      display, display::DisplayObserver::DISPLAY_METRIC_ROTATION);

  display.set_rotation(display::Display::ROTATE_90);
  display.set_panel_rotation(display::Display::ROTATE_180);
  EXPECT_CALL(*mock_aura_output,
              SendLogicalTransform(OutputTransform(display.rotation())))
      .Times(1);
  mock_aura_output->SendDisplayMetrics(
      display, display::DisplayObserver::DISPLAY_METRIC_ROTATION);

  display.set_rotation(display::Display::ROTATE_270);
  display.set_panel_rotation(display::Display::ROTATE_270);
  EXPECT_CALL(*mock_aura_output,
              SendLogicalTransform(OutputTransform(display.rotation())))
      .Times(1);
  mock_aura_output->SendDisplayMetrics(
      display, display::DisplayObserver::DISPLAY_METRIC_ROTATION);
}

}  // namespace wayland
}  // namespace exo
