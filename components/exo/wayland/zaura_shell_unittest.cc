// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/zaura_shell.h"

#include <memory>

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/window_util.h"
#include "base/time/time.h"
#include "components/exo/test/exo_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window_occlusion_tracker.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/compositor/test/layer_animator_test_controller.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_change_observer.h"
#include "ui/wm/public/activation_client.h"

namespace exo {
namespace wayland {

namespace {

constexpr auto kTransitionDuration = base::TimeDelta::FromSeconds(3);

class TestAuraSurface : public AuraSurface {
 public:
  explicit TestAuraSurface(Surface* surface)
      : AuraSurface(surface, /*resource=*/nullptr) {}

  float last_sent_occlusion_fraction() const {
    return last_sent_occlusion_fraction_;
  }
  int num_occlusion_updates() const { return num_occlusion_updates_; }

 protected:
  void SendOcclusionFraction(float occlusion_fraction) override {
    last_sent_occlusion_fraction_ = occlusion_fraction;
    num_occlusion_updates_++;
  }

 private:
  float last_sent_occlusion_fraction_ = -1.0f;
  int num_occlusion_updates_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestAuraSurface);
};

}  // namespace

class ZAuraSurfaceTest : public test::ExoTestBase,
                         public ::wm::ActivationChangeObserver {
 public:
  ZAuraSurfaceTest() {}
  ~ZAuraSurfaceTest() override {}

  // test::ExoTestBase overrides:
  void SetUp() override {
    test::ExoTestBase::SetUp();

    surface_.reset(new Surface);
    aura_surface_.reset(new TestAuraSurface(surface_.get()));

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

  DISALLOW_COPY_AND_ASSIGN(ZAuraSurfaceTest);
};

TEST_F(ZAuraSurfaceTest,
       LosingActivationWithNoAnimatingWindowsSendsCorrectOcclusionFraction) {
  EXPECT_EQ(0.0f, aura_surface().last_sent_occlusion_fraction());
  EXPECT_EQ(1, aura_surface().num_occlusion_updates());
  ::wm::ActivateWindow(parent_widget().GetNativeWindow());

  // Creating an opaque window but don't show it.
  auto widget = CreateOpaqueWidget(gfx::Rect(0, 0, 10, 10));

  // Occlusion sent before de-activation should include that widget.
  widget->Show();
  EXPECT_EQ(0.2f, occlusion_fraction_on_activation_loss());
  EXPECT_EQ(0.2f, aura_surface().last_sent_occlusion_fraction());
  EXPECT_EQ(2, aura_surface().num_occlusion_updates());
}

TEST_F(ZAuraSurfaceTest,
       LosingActivationWithAnimatingWindowsSendsTargetOcclusionFraction) {
  EXPECT_EQ(0.0f, aura_surface().last_sent_occlusion_fraction());
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
  EXPECT_EQ(3, aura_surface().num_occlusion_updates());
}

TEST_F(ZAuraSurfaceTest,
       LosingActivationByTriggeringTheLockScreenDoesNotSendOccludedFraction) {
  EXPECT_EQ(0.0f, aura_surface().last_sent_occlusion_fraction());
  EXPECT_EQ(1, aura_surface().num_occlusion_updates());
  ::wm::ActivateWindow(parent_widget().GetNativeWindow());

  // Lock the screen.
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  auto lock_widget = std::make_unique<views::Widget>();
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.context = CurrentContext();
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
}

TEST_F(ZAuraSurfaceTest, OcclusionIncludesOffScreenArea) {
  UpdateDisplay("150x150");
  // This is scaled by 1.5 - set the bounds to (-60, 75, 120, 150) in screen
  // coordinates so 75% of it is outside of the 100x100 screen.
  surface().window()->SetBounds(gfx::Rect(-40, 50, 80, 100));
  surface().OnWindowOcclusionChanged();

  EXPECT_EQ(0.75f, aura_surface().last_sent_occlusion_fraction());
}

TEST_F(ZAuraSurfaceTest, ZeroSizeWindowSendsZeroOcclusionFraction) {
  // Zero sized window should not be occluded.
  surface().window()->SetBounds(gfx::Rect(0, 0, 0, 0));
  surface().OnWindowOcclusionChanged();
  EXPECT_EQ(0.0f, aura_surface().last_sent_occlusion_fraction());
}

}  // namespace wayland
}  // namespace exo
