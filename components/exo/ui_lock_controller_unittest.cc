// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/ui_lock_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/wm/window_state.h"
#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/exo/buffer.h"
#include "components/exo/display.h"
#include "components/exo/pointer.h"
#include "components/exo/pointer_constraint_delegate.h"
#include "components/exo/pointer_delegate.h"
#include "components/exo/shell_surface.h"
#include "components/exo/surface.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/exo_test_helper.h"
#include "components/exo/wm_helper.h"
#include "components/fullscreen_control/fullscreen_control_popup.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/class_property.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/wm/core/window_util.h"

namespace exo {

namespace {

constexpr char kNoEscHoldAppId[] = "no-esc-hold";
constexpr char kOverviewToExitAppId[] = "overview-to-exit";

struct SurfaceTriplet {
  std::unique_ptr<Surface> surface;
  std::unique_ptr<ShellSurface> shell_surface;
  std::unique_ptr<Buffer> buffer;

  aura::Window* GetAlwaysOnTopContainer() {
    aura::Window* native_window = GetTopLevelWidget()->GetNativeWindow();
    return ash::Shell::GetContainer(native_window->GetRootWindow(),
                                    ash::kShellWindowId_AlwaysOnTopContainer);
  }

  views::Widget* GetTopLevelWidget() {
    views::Widget* top_level_widget =
        views::Widget::GetTopLevelWidgetForNativeView(surface->window());
    assert(top_level_widget);
    return top_level_widget;
  }

  aura::Window* GetTopLevelWindow() {
    auto* top_level_widget = views::Widget::GetTopLevelWidgetForNativeView(
        shell_surface->host_window());
    assert(top_level_widget);
    return top_level_widget->GetNativeWindow();
  }

  ash::WindowState* GetTopLevelWindowState() {
    return ash::WindowState::Get(GetTopLevelWindow());
  }
};

class MockPointerDelegate : public PointerDelegate {
 public:
  MockPointerDelegate(Surface* surface) {
    EXPECT_CALL(*this, CanAcceptPointerEventsForSurface(surface))
        .WillRepeatedly(testing::Return(true));
  }

  // Overridden from PointerDelegate:
  MOCK_METHOD1(OnPointerDestroying, void(Pointer*));
  MOCK_CONST_METHOD1(CanAcceptPointerEventsForSurface, bool(Surface*));
  MOCK_METHOD3(OnPointerEnter, void(Surface*, const gfx::PointF&, int));
  MOCK_METHOD1(OnPointerLeave, void(Surface*));
  MOCK_METHOD2(OnPointerMotion, void(base::TimeTicks, const gfx::PointF&));
  MOCK_METHOD3(OnPointerButton, void(base::TimeTicks, int, bool));
  MOCK_METHOD3(OnPointerScroll,
               void(base::TimeTicks, const gfx::Vector2dF&, bool));
  MOCK_METHOD1(OnPointerScrollStop, void(base::TimeTicks));
  MOCK_METHOD0(OnPointerFrame, void());
};

class MockPointerConstraintDelegate : public PointerConstraintDelegate {
 public:
  MockPointerConstraintDelegate(Pointer* pointer, Surface* surface)
      : pointer_(pointer) {
    EXPECT_CALL(*this, GetConstrainedSurface())
        .WillRepeatedly(testing::Return(surface));
    ON_CALL(*this, OnConstraintActivated).WillByDefault([this]() {
      activated_count++;
    });
    ON_CALL(*this, OnConstraintBroken).WillByDefault([this]() {
      broken_count++;
    });
  }

  ~MockPointerConstraintDelegate() {
    // Notifying destruction here removes some boilerplate from tests.
    pointer_->OnPointerConstraintDelegateDestroying(this);
  }

  // Overridden from PointerConstraintDelegate:
  MOCK_METHOD0(OnConstraintActivated, void());
  MOCK_METHOD0(OnAlreadyConstrained, void());
  MOCK_METHOD0(OnConstraintBroken, void());
  MOCK_METHOD0(IsPersistent, bool());
  MOCK_METHOD0(GetConstrainedSurface, Surface*());
  MOCK_METHOD0(OnDefunct, void());

  raw_ptr<Pointer> pointer_;
  int activated_count = 0;
  int broken_count = 0;
};

class UILockControllerTest : public test::ExoTestBase {
 public:
  UILockControllerTest()
      : test::ExoTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~UILockControllerTest() override = default;

  UILockControllerTest(const UILockControllerTest&) = delete;
  UILockControllerTest& operator=(const UILockControllerTest&) = delete;

 protected:
  class TestPropertyResolver : public exo::WMHelper::AppPropertyResolver {
   public:
    TestPropertyResolver() = default;
    ~TestPropertyResolver() override = default;
    void PopulateProperties(
        const Params& params,
        ui::PropertyHandler& out_properties_container) override {
      out_properties_container.SetProperty(
          chromeos::kEscHoldToExitFullscreen,
          params.app_id != kNoEscHoldAppId &&
              params.app_id != kOverviewToExitAppId);
      out_properties_container.SetProperty(
          chromeos::kUseOverviewToExitFullscreen,
          params.app_id == kOverviewToExitAppId);
      out_properties_container.SetProperty(
          chromeos::kUseOverviewToExitPointerLock,
          params.app_id == kOverviewToExitAppId);
    }
  };

  // test::ExoTestBase:
  void SetUp() override {
    test::ExoTestBase::SetUp();
    seat_ = std::make_unique<Seat>();
    scoped_feature_list_.InitWithFeatures(
        {chromeos::features::kExoLockNotification,
         chromeos::features::kExoPointerLock},
        {});
    WMHelper::GetInstance()->RegisterAppPropertyResolver(
        std::make_unique<TestPropertyResolver>());
  }

  void TearDown() override {
    seat_.reset();
    test::ExoTestBase::TearDown();
  }

  SurfaceTriplet BuildSurface(gfx::Point origin, int w, int h) {
    auto surface = std::make_unique<Surface>();
    auto shell_surface = std::make_unique<ShellSurface>(
        surface.get(), origin,
        /*can_minimize=*/true, ash::desks_util::GetActiveDeskContainerId());
    auto buffer = std::make_unique<Buffer>(
        exo_test_helper()->CreateGpuMemoryBuffer({w, h}));
    surface->Attach(buffer.get());

    return {std::move(surface), std::move(shell_surface), std::move(buffer)};
  }

  SurfaceTriplet BuildSurface(int w, int h) {
    return BuildSurface(gfx::Point(0, 0), w, h);
  }

  views::Widget* GetEscNotification(SurfaceTriplet* surface) {
    return seat_->GetUILockControllerForTesting()->GetEscNotificationForTesting(
        surface->GetTopLevelWindow());
  }

  views::Widget* GetPointerCaptureNotification(SurfaceTriplet* surface) {
    return seat_->GetUILockControllerForTesting()
        ->GetPointerCaptureNotificationForTesting(surface->GetTopLevelWindow());
  }

  bool IsExitPopupVisible(aura::Window* window) {
    FullscreenControlPopup* popup =
        seat_->GetUILockControllerForTesting()->GetExitPopupForTesting(window);
    if (popup && popup->IsAnimating()) {
      gfx::AnimationTestApi animation_api(popup->GetAnimationForTesting());
      base::TimeTicks now = base::TimeTicks::Now();
      animation_api.SetStartTime(now);
      animation_api.Step(now + base::Milliseconds(500));
    }
    return popup && popup->IsVisible();
  }

  std::unique_ptr<Seat> seat_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(UILockControllerTest, HoldingEscapeExitsFullscreen) {
  SurfaceTriplet test_surface = BuildSurface(1024, 768);
  test_surface.shell_surface->SetUseImmersiveForFullscreen(false);
  test_surface.shell_surface->SetFullscreen(true);
  test_surface.surface->Commit();
  auto* window_state = test_surface.GetTopLevelWindowState();
  EXPECT_TRUE(window_state->IsFullscreen());

  GetEventGenerator()->PressKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(window_state->IsFullscreen());  // no change yet

  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(window_state->IsFullscreen());
  EXPECT_TRUE(window_state->IsNormalStateType());
}

TEST_F(UILockControllerTest, HoldingCtrlEscapeDoesNotExitFullscreen) {
  SurfaceTriplet test_surface = BuildSurface(1024, 768);
  test_surface.shell_surface->SetUseImmersiveForFullscreen(false);
  test_surface.shell_surface->SetFullscreen(true);
  test_surface.surface->Commit();
  auto* window_state = test_surface.GetTopLevelWindowState();
  EXPECT_TRUE(window_state->IsFullscreen());

  GetEventGenerator()->PressKey(ui::VKEY_ESCAPE, ui::EF_CONTROL_DOWN);
  task_environment()->FastForwardBy(base::Seconds(2));
  EXPECT_TRUE(window_state->IsFullscreen());
}

TEST_F(UILockControllerTest,
       HoldingEscapeOnlyExitsFullscreenIfWindowPropertySet) {
  SurfaceTriplet test_surface = BuildSurface(1024, 768);
  // Do not set chromeos::kEscHoldToExitFullscreen on TopLevelWindow.
  test_surface.shell_surface->SetApplicationId(kNoEscHoldAppId);
  test_surface.shell_surface->SetUseImmersiveForFullscreen(false);
  test_surface.shell_surface->SetFullscreen(true);
  test_surface.surface->Commit();
  auto* window_state = test_surface.GetTopLevelWindowState();
  EXPECT_TRUE(window_state->IsFullscreen());

  GetEventGenerator()->PressKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  task_environment()->FastForwardBy(base::Seconds(2));
  EXPECT_TRUE(window_state->IsFullscreen());
}

TEST_F(UILockControllerTest, HoldingEscapeOnlyExitsFocusedFullscreen) {
  SurfaceTriplet test_surface1 = BuildSurface(1024, 768);
  test_surface1.shell_surface->SetUseImmersiveForFullscreen(false);
  test_surface1.shell_surface->SetFullscreen(true);
  test_surface1.surface->Commit();

  SurfaceTriplet test_surface2 = BuildSurface(1024, 768);
  test_surface2.shell_surface->SetUseImmersiveForFullscreen(false);
  test_surface2.shell_surface->SetFullscreen(true);
  test_surface2.surface->Commit();

  GetEventGenerator()->PressKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  task_environment()->FastForwardBy(base::Seconds(2));

  EXPECT_TRUE(test_surface1.GetTopLevelWindowState()->IsFullscreen());
  EXPECT_FALSE(test_surface2.GetTopLevelWindowState()->IsFullscreen());
}

TEST_F(UILockControllerTest, DestroyingWindowCancels) {
  std::unique_ptr<SurfaceTriplet> test_surface =
      std::make_unique<SurfaceTriplet>(BuildSurface(1024, 768));
  test_surface->shell_surface->SetUseImmersiveForFullscreen(false);
  test_surface->shell_surface->SetFullscreen(true);
  test_surface->surface->Commit();
  auto* window_state = test_surface->GetTopLevelWindowState();
  EXPECT_TRUE(window_state->IsFullscreen());

  GetEventGenerator()->PressKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  task_environment()->FastForwardBy(base::Seconds(1));

  test_surface.reset();  // Destroying the Surface destroys the Window

  task_environment()->FastForwardBy(base::Seconds(3));

  // The implicit assertion is that the code doesn't crash.
}

TEST_F(UILockControllerTest, FocusChangeCancels) {
  // Arrange: two windows, one is fullscreen and focused
  SurfaceTriplet other_surface = BuildSurface(1024, 768);
  other_surface.surface->Commit();

  SurfaceTriplet fullscreen_surface = BuildSurface(1024, 768);
  fullscreen_surface.shell_surface->SetUseImmersiveForFullscreen(false);
  fullscreen_surface.shell_surface->SetFullscreen(true);
  fullscreen_surface.surface->Commit();

  EXPECT_EQ(fullscreen_surface.surface.get(), seat_->GetFocusedSurface());
  EXPECT_FALSE(fullscreen_surface.GetTopLevelWindowState()->IsMinimized());

  // Act: Press escape, then toggle focus back and forth
  GetEventGenerator()->PressKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  task_environment()->FastForwardBy(base::Seconds(1));

  wm::ActivateWindow(other_surface.surface->window());
  wm::ActivateWindow(fullscreen_surface.surface->window());

  task_environment()->FastForwardBy(base::Seconds(2));

  // Assert: Fullscreen window was not minimized, despite regaining focus.
  EXPECT_FALSE(fullscreen_surface.GetTopLevelWindowState()->IsMinimized());
  EXPECT_EQ(fullscreen_surface.surface.get(), seat_->GetFocusedSurface());
}

TEST_F(UILockControllerTest, ShortHoldEscapeDoesNotExitFullscreen) {
  SurfaceTriplet test_surface = BuildSurface(1024, 768);
  test_surface.shell_surface->SetUseImmersiveForFullscreen(false);
  test_surface.shell_surface->SetFullscreen(true);
  test_surface.surface->Commit();
  auto* window_state = test_surface.GetTopLevelWindowState();

  GetEventGenerator()->PressKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  task_environment()->FastForwardBy(base::Seconds(1));
  GetEventGenerator()->ReleaseKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  task_environment()->FastForwardBy(base::Seconds(2));

  EXPECT_TRUE(window_state->IsFullscreen());
}

TEST_F(UILockControllerTest, FullScreenShowsEscNotification) {
  SurfaceTriplet test_surface = BuildSurface(1024, 768);
  test_surface.shell_surface->SetUseImmersiveForFullscreen(false);
  test_surface.shell_surface->SetFullscreen(true);
  test_surface.surface->Commit();

  EXPECT_TRUE(test_surface.GetTopLevelWindowState()->IsFullscreen());
  EXPECT_TRUE(GetEscNotification(&test_surface));
}

TEST_F(UILockControllerTest, EscNotificationClosesAfterDuration) {
  SurfaceTriplet test_surface = BuildSurface(1024, 768);
  test_surface.shell_surface->SetUseImmersiveForFullscreen(false);
  test_surface.shell_surface->SetFullscreen(true);
  test_surface.surface->Commit();

  EXPECT_TRUE(GetEscNotification(&test_surface));
  task_environment()->FastForwardBy(base::Seconds(5));
  EXPECT_FALSE(GetEscNotification(&test_surface));
}

TEST_F(UILockControllerTest, HoldingEscapeHidesNotification) {
  SurfaceTriplet test_surface = BuildSurface(1024, 768);
  test_surface.shell_surface->SetUseImmersiveForFullscreen(false);
  test_surface.shell_surface->SetFullscreen(true);
  test_surface.surface->Commit();

  EXPECT_TRUE(test_surface.GetTopLevelWindowState()->IsFullscreen());
  EXPECT_TRUE(GetEscNotification(&test_surface));

  GetEventGenerator()->PressKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  task_environment()->FastForwardBy(base::Seconds(3));

  EXPECT_FALSE(test_surface.GetTopLevelWindowState()->IsFullscreen());
  EXPECT_FALSE(GetEscNotification(&test_surface));
}

TEST_F(UILockControllerTest, LosingFullscreenHidesNotification) {
  SurfaceTriplet test_surface = BuildSurface(1024, 768);
  test_surface.shell_surface->SetUseImmersiveForFullscreen(false);
  test_surface.shell_surface->SetFullscreen(true);
  test_surface.surface->Commit();

  EXPECT_TRUE(test_surface.GetTopLevelWindowState()->IsFullscreen());
  EXPECT_TRUE(GetEscNotification(&test_surface));

  // Have surface loose fullscreen, notification should now be hidden.
  test_surface.shell_surface->Minimize();
  test_surface.shell_surface->SetFullscreen(false);
  test_surface.surface->Commit();

  EXPECT_FALSE(test_surface.GetTopLevelWindowState()->IsFullscreen());
  EXPECT_FALSE(
      seat_->GetUILockControllerForTesting()->GetEscNotificationForTesting(
          test_surface.GetTopLevelWindow()));
}

TEST_F(UILockControllerTest, EscNotificationIsReshown) {
  SurfaceTriplet test_surface = BuildSurface(1024, 768);
  test_surface.shell_surface->SetUseImmersiveForFullscreen(false);
  test_surface.shell_surface->SetFullscreen(true);
  test_surface.surface->Commit();

  EXPECT_TRUE(GetEscNotification(&test_surface));

  // Stop fullscreen.
  test_surface.shell_surface->SetFullscreen(false);
  EXPECT_FALSE(
      seat_->GetUILockControllerForTesting()->GetEscNotificationForTesting(
          test_surface.GetTopLevelWindow()));

  // Fullscreen should show notification since it did not stay visible for
  // duration.
  test_surface.shell_surface->SetFullscreen(true);
  EXPECT_TRUE(GetEscNotification(&test_surface));

  // After duration, notification should be removed.
  task_environment()->FastForwardBy(base::Seconds(5));
  EXPECT_FALSE(GetEscNotification(&test_surface));

  // Notification is shown after fullscreen toggle.
  test_surface.shell_surface->SetFullscreen(false);
  test_surface.shell_surface->SetFullscreen(true);
  EXPECT_TRUE(GetEscNotification(&test_surface));
}

TEST_F(UILockControllerTest, EscNotificationShowsOnSecondaryDisplay) {
  // Create surface on secondary display.
  UpdateDisplay("900x800,70x600");
  SurfaceTriplet test_surface = BuildSurface(gfx::Point(900, 100), 200, 200);
  test_surface.shell_surface->SetUseImmersiveForFullscreen(false);
  test_surface.shell_surface->SetFullscreen(true);
  test_surface.surface->Commit();

  // Esc notification should be in secondary display.
  views::Widget* esc_notification = GetEscNotification(&test_surface);
  EXPECT_TRUE(GetSecondaryDisplay().bounds().Contains(
      esc_notification->GetWindowBoundsInScreen()));
}

TEST_F(UILockControllerTest, PointerLockShowsNotification) {
  SurfaceTriplet test_surface = BuildSurface(1024, 768);
  test_surface.shell_surface->SetApplicationId(kOverviewToExitAppId);
  test_surface.surface->Commit();
  testing::NiceMock<MockPointerDelegate> delegate(test_surface.surface.get());
  Pointer pointer(&delegate, seat_.get());
  testing::NiceMock<MockPointerConstraintDelegate> constraint(
      &pointer, test_surface.surface.get());
  EXPECT_FALSE(GetPointerCaptureNotification(&test_surface));

  EXPECT_TRUE(pointer.ConstrainPointer(&constraint));

  EXPECT_TRUE(GetPointerCaptureNotification(&test_surface));
}

TEST_F(UILockControllerTest, PointerLockNotificationObeysCooldown) {
  // Arrange: Set up a pointer capture notification.
  SurfaceTriplet test_surface = BuildSurface(1024, 768);
  test_surface.shell_surface->SetApplicationId(kOverviewToExitAppId);
  test_surface.surface->Commit();
  testing::NiceMock<MockPointerDelegate> delegate(test_surface.surface.get());
  Pointer pointer(&delegate, seat_.get());
  testing::NiceMock<MockPointerConstraintDelegate> constraint(
      &pointer, test_surface.surface.get());
  EXPECT_TRUE(pointer.ConstrainPointer(&constraint));
  EXPECT_TRUE(GetPointerCaptureNotification(&test_surface));

  // Act: Wait for the notification to timeout.
  task_environment()->FastForwardBy(base::Seconds(5));

  // Assert: Notification has disappeared.
  EXPECT_FALSE(GetPointerCaptureNotification(&test_surface));

  // Act: Remove and re-apply the constraint.
  pointer.OnPointerConstraintDelegateDestroying(&constraint);
  EXPECT_TRUE(pointer.ConstrainPointer(&constraint));

  // Assert: Notification not shown due to the cooldown.
  EXPECT_FALSE(GetPointerCaptureNotification(&test_surface));

  // Act: Wait for the cooldown, then re-apply again
  pointer.OnPointerConstraintDelegateDestroying(&constraint);
  task_environment()->FastForwardBy(base::Minutes(5));
  EXPECT_TRUE(pointer.ConstrainPointer(&constraint));

  // Assert: Cooldown has expired so notification is shown.
  EXPECT_TRUE(GetPointerCaptureNotification(&test_surface));
}

TEST_F(UILockControllerTest, FullscreenNotificationHasPriority) {
  // Arrange: Set up a pointer capture notification.
  SurfaceTriplet test_surface = BuildSurface(1024, 768);
  test_surface.shell_surface->SetApplicationId(kOverviewToExitAppId);
  test_surface.surface->Commit();
  testing::NiceMock<MockPointerDelegate> delegate(test_surface.surface.get());
  Pointer pointer(&delegate, seat_.get());
  testing::NiceMock<MockPointerConstraintDelegate> constraint(
      &pointer, test_surface.surface.get());
  EXPECT_TRUE(pointer.ConstrainPointer(&constraint));
  EXPECT_TRUE(GetPointerCaptureNotification(&test_surface));

  // Act: Go fullscreen.
  test_surface.shell_surface->SetUseImmersiveForFullscreen(false);
  test_surface.shell_surface->SetFullscreen(true);
  test_surface.surface->Commit();

  // Assert: Fullscreen notification overrides pointer notification.
  EXPECT_FALSE(GetPointerCaptureNotification(&test_surface));
  EXPECT_TRUE(GetEscNotification(&test_surface));

  // Act: Exit fullscreen.
  test_surface.shell_surface->SetFullscreen(false);
  test_surface.surface->Commit();

  // Assert: Pointer notification returns, since it was interrupted.
  EXPECT_TRUE(GetPointerCaptureNotification(&test_surface));
  EXPECT_FALSE(GetEscNotification(&test_surface));
}

TEST_F(UILockControllerTest, ExitPopup) {
  SurfaceTriplet test_surface = BuildSurface(1024, 768);
  test_surface.shell_surface->SetUseImmersiveForFullscreen(false);
  test_surface.shell_surface->SetFullscreen(true);
  test_surface.surface->Commit();
  auto* window_state = test_surface.GetTopLevelWindowState();
  EXPECT_TRUE(window_state->IsFullscreen());
  aura::Window* window = test_surface.GetTopLevelWindow();
  EXPECT_FALSE(IsExitPopupVisible(window));
  EXPECT_TRUE(GetEscNotification(&test_surface));

  // Move mouse above y=3 should not show exit popup while notification is
  // visible.
  GetEventGenerator()->MoveMouseTo(0, 2);
  EXPECT_FALSE(IsExitPopupVisible(window));

  // Wait for notification to close, now exit popup should show.
  task_environment()->FastForwardBy(base::Seconds(5));
  EXPECT_FALSE(GetEscNotification(&test_surface));
  GetEventGenerator()->MoveMouseTo(1, 2);
  EXPECT_TRUE(IsExitPopupVisible(window));

  // Move mouse below y=150 should hide exit popup.
  GetEventGenerator()->MoveMouseTo(0, 160);
  EXPECT_FALSE(IsExitPopupVisible(window));

  // Move mouse back above y=3 should show exit popup.
  GetEventGenerator()->MoveMouseTo(0, 2);
  EXPECT_TRUE(IsExitPopupVisible(window));

  // Popup should hide after 3s.
  task_environment()->FastForwardBy(base::Seconds(5));
  EXPECT_FALSE(IsExitPopupVisible(window));

  // Moving mouse to y=100, then above y=3 should still have popup hidden.
  GetEventGenerator()->MoveMouseTo(0, 100);
  GetEventGenerator()->MoveMouseTo(0, 2);
  EXPECT_FALSE(IsExitPopupVisible(window));

  // Moving mouse below y=150, then above y=3 should show exit popup.
  GetEventGenerator()->MoveMouseTo(0, 160);
  GetEventGenerator()->MoveMouseTo(0, 2);
  EXPECT_TRUE(IsExitPopupVisible(window));

  // Clicking exit popup should exit fullscreen.
  FullscreenControlPopup* popup =
      seat_->GetUILockControllerForTesting()->GetExitPopupForTesting(window);
  GetEventGenerator()->MoveMouseTo(
      popup->GetPopupWidget()->GetWindowBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();
  EXPECT_FALSE(window_state->IsFullscreen());
  EXPECT_FALSE(IsExitPopupVisible(window));
}

TEST_F(UILockControllerTest, ExitPopupNotShownForOverviewCase) {
  SurfaceTriplet test_surface = BuildSurface(1024, 768);
  // Set chromeos::kUseOverviewToExitFullscreen on TopLevelWindow.
  test_surface.shell_surface->SetApplicationId(kOverviewToExitAppId);
  test_surface.shell_surface->SetUseImmersiveForFullscreen(false);
  test_surface.shell_surface->SetFullscreen(true);
  test_surface.surface->Commit();
  EXPECT_FALSE(IsExitPopupVisible(test_surface.GetTopLevelWindow()));

  // Move mouse above y=3 should not show exit popup.
  GetEventGenerator()->MoveMouseTo(0, 2);
  EXPECT_FALSE(IsExitPopupVisible(test_surface.GetTopLevelWindow()));
}

TEST_F(UILockControllerTest, OnlyShowWhenActive) {
  SurfaceTriplet test_surface1 = BuildSurface(1024, 768);
  test_surface1.surface->Commit();
  SurfaceTriplet test_surface2 = BuildSurface(gfx::Point(100, 100), 200, 200);
  test_surface2.surface->Commit();

  // Surface2 is active when we make Surface1 fullscreen.
  // Esc notification, and exit popup should not be shown.
  test_surface1.shell_surface->SetFullscreen(true);
  EXPECT_FALSE(GetEscNotification(&test_surface1));
  GetEventGenerator()->MoveMouseTo(0, 2);
  EXPECT_FALSE(IsExitPopupVisible(test_surface1.GetTopLevelWindow()));
}

}  // namespace
}  // namespace exo
