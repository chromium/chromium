// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/ui_lock_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/wm/window_state.h"
#include "base/feature_list.h"
#include "base/test/power_monitor_test.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/login/auth/auth_events_recorder.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"
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
#include "components/exo/test/shell_surface_builder.h"
#include "components/exo/wm_helper.h"
#include "components/fullscreen_control/fullscreen_control_popup.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/class_property.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/wm/core/window_util.h"

namespace exo {

namespace {

constexpr char kNoEscHoldAppId[] = "no-esc-hold";
constexpr char kOverviewToExitAppId[] = "overview-to-exit";

aura::Window* GetTopLevelWindow(
    const std::unique_ptr<ShellSurface>& shell_surface) {
  auto* top_level_widget = views::Widget::GetTopLevelWidgetForNativeView(
      shell_surface->host_window());
  assert(top_level_widget);
  return top_level_widget->GetNativeWindow();
}

ash::WindowState* GetTopLevelWindowState(
    const std::unique_ptr<ShellSurface>& shell_surface) {
  return ash::WindowState::Get(GetTopLevelWindow(shell_surface));
}

class MockPointerDelegate : public PointerDelegate {
 public:
  MockPointerDelegate() {
    EXPECT_CALL(*this, CanAcceptPointerEventsForSurface(testing::_))
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
  MOCK_METHOD1(OnFingerScrollStop, void(base::TimeTicks));
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

  ~MockPointerConstraintDelegate() override {
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

    // Order of window activations and observer callbacks is not trivial, e.g.
    // lock screen widget is active when `OnLockStateChanged(locked=false)`
    // callback is called. It's better to test them with views.
    // `AuthEventsRecorder` is required for `set_show_lock_screen_views=true`.
    auth_events_recorder_ = ash::AuthEventsRecorder::CreateForTesting();
    GetSessionControllerClient()->set_show_lock_screen_views(true);

    WMHelper::GetInstance()->RegisterAppPropertyResolver(
        std::make_unique<TestPropertyResolver>());
  }

  void TearDown() override {
    seat_.reset();
    test::ExoTestBase::TearDown();
  }

  std::unique_ptr<ShellSurface> BuildSurface(gfx::Point origin, int w, int h) {
    test::ShellSurfaceBuilder builder({w, h});
    builder.SetOrigin(origin);
    return builder.BuildShellSurface();
  }

  std::unique_ptr<ShellSurface> BuildSurface(int w, int h) {
    return BuildSurface(gfx::Point(0, 0), w, h);
  }

  views::Widget* GetEscNotification(
      const std::unique_ptr<ShellSurface>& surface) {
    return seat_->GetUILockControllerForTesting()->GetEscNotificationForTesting(
        GetTopLevelWindow(surface));
  }

  views::Widget* GetPointerCaptureNotification(
      const std::unique_ptr<ShellSurface>& surface) {
    return seat_->GetUILockControllerForTesting()
        ->GetPointerCaptureNotificationForTesting(GetTopLevelWindow(surface));
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
  std::unique_ptr<ash::AuthEventsRecorder> auth_events_recorder_;
};

TEST_F(UILockControllerTest, HoldingEscapeExitsFullscreen) {
  std::unique_ptr<ShellSurface> test_surface = BuildSurface(600, 400);
  test_surface->SetUseImmersiveForFullscreen(false);
  test_surface->SetFullscreen(true, display::kInvalidDisplayId);
  test_surface->surface_for_testing()->Commit();
  auto* window_state = GetTopLevelWindowState(test_surface);
  EXPECT_TRUE(window_state->IsFullscreen());

  GetEventGenerator()->PressKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(window_state->IsFullscreen());  // no change yet

  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(window_state->IsFullscreen());
  EXPECT_TRUE(window_state->IsNormalStateType());
}

TEST_F(UILockControllerTest, HoldingCtrlEscapeDoesNotExitFullscreen) {
  std::unique_ptr<ShellSurface> test_surface = BuildSurface(1024, 768);
  test_surface->SetUseImmersiveForFullscreen(false);
  test_surface->SetFullscreen(true, display::kInvalidDisplayId);
  test_surface->surface_for_testing()->Commit();
  auto* window_state = GetTopLevelWindowState(test_surface);
  EXPECT_TRUE(window_state->IsFullscreen());

  GetEventGenerator()->PressKey(ui::VKEY_ESCAPE, ui::EF_CONTROL_DOWN);
  task_environment()->FastForwardBy(base::Seconds(2));
  EXPECT_TRUE(window_state->IsFullscreen());
}

TEST_F(UILockControllerTest,
       HoldingEscapeOnlyExitsFullscreenIfWindowPropertySet) {
  std::unique_ptr<ShellSurface> test_surface = BuildSurface(1024, 768);
  // Do not set chromeos::kEscHoldToExitFullscreen on TopLevelWindow.
  test_surface->SetApplicationId(kNoEscHoldAppId);
  test_surface->SetUseImmersiveForFullscreen(false);
  test_surface->SetFullscreen(true, display::kInvalidDisplayId);
  test_surface->surface_for_testing()->Commit();
  auto* window_state = GetTopLevelWindowState(test_surface);
  EXPECT_TRUE(window_state->IsFullscreen());

  GetEventGenerator()->PressKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  task_environment()->FastForwardBy(base::Seconds(2));
  EXPECT_TRUE(window_state->IsFullscreen());
}

TEST_F(UILockControllerTest, HoldingEscapeOnlyExitsFocusedFullscreen) {
  std::unique_ptr<ShellSurface> test_surface1 = BuildSurface(1024, 768);
  test_surface1->SetUseImmersiveForFullscreen(false);
  test_surface1->SetFullscreen(true, display::kInvalidDisplayId);
  test_surface1->surface_for_testing()->Commit();

  std::unique_ptr<ShellSurface> test_surface2 = BuildSurface(1024, 768);
  test_surface2->SetUseImmersiveForFullscreen(false);
  test_surface2->SetFullscreen(true, display::kInvalidDisplayId);
  test_surface2->surface_for_testing()->Commit();

  GetEventGenerator()->PressKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  task_environment()->FastForwardBy(base::Seconds(2));

  EXPECT_TRUE(GetTopLevelWindowState(test_surface1)->IsFullscreen());
  EXPECT_FALSE(GetTopLevelWindowState(test_surface2)->IsFullscreen());
}

TEST_F(UILockControllerTest, DestroyingWindowCancels) {
  std::unique_ptr<ShellSurface> test_surface = BuildSurface(1024, 768);
  test_surface->SetUseImmersiveForFullscreen(false);
  test_surface->SetFullscreen(true, display::kInvalidDisplayId);
  test_surface->surface_for_testing()->Commit();
  auto* window_state = GetTopLevelWindowState(test_surface);
  EXPECT_TRUE(window_state->IsFullscreen());

  GetEventGenerator()->PressKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  task_environment()->FastForwardBy(base::Seconds(1));

  test_surface.reset();  // Destroying the Surface destroys the Window

  task_environment()->FastForwardBy(base::Seconds(3));

  // The implicit assertion is that the code doesn't crash.
}

TEST_F(UILockControllerTest, FocusChangeCancels) {
  // Arrange: two windows, one is fullscreen and focused
  std::unique_ptr<ShellSurface> other_surface = BuildSurface(1024, 768);
  other_surface->surface_for_testing()->Commit();

  std::unique_ptr<ShellSurface> fullscreen_surface = BuildSurface(1024, 768);
  fullscreen_surface->SetUseImmersiveForFullscreen(false);
  fullscreen_surface->SetFullscreen(true, display::kInvalidDisplayId);
  fullscreen_surface->surface_for_testing()->Commit();

  EXPECT_EQ(fullscreen_surface->surface_for_testing(),
            seat_->GetFocusedSurface());
  EXPECT_FALSE(GetTopLevelWindowState(fullscreen_surface)->IsMinimized());

  // Act: Press escape, then toggle focus back and forth
  GetEventGenerator()->PressKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  task_environment()->FastForwardBy(base::Seconds(1));

  wm::ActivateWindow(other_surface->surface_for_testing()->window());
  wm::ActivateWindow(fullscreen_surface->surface_for_testing()->window());

  task_environment()->FastForwardBy(base::Seconds(2));

  // Assert: Fullscreen window was not minimized, despite regaining focus.
  EXPECT_FALSE(GetTopLevelWindowState(fullscreen_surface)->IsMinimized());
  EXPECT_EQ(fullscreen_surface->surface_for_testing(),
            seat_->GetFocusedSurface());
}

TEST_F(UILockControllerTest, ShortHoldEscapeDoesNotExitFullscreen) {
  std::unique_ptr<ShellSurface> test_surface = BuildSurface(1024, 768);
  test_surface->SetUseImmersiveForFullscreen(false);
  test_surface->SetFullscreen(true, display::kInvalidDisplayId);
  test_surface->surface_for_testing()->Commit();
  auto* window_state = GetTopLevelWindowState(test_surface);

  GetEventGenerator()->PressKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  task_environment()->FastForwardBy(base::Seconds(1));
  GetEventGenerator()->ReleaseKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  task_environment()->FastForwardBy(base::Seconds(2));

  EXPECT_TRUE(window_state->IsFullscreen());
}

TEST_F(UILockControllerTest, FullScreenShowsEscNotification) {
  std::unique_ptr<ShellSurface> test_surface = BuildSurface(1024, 768);
  test_surface->SetUseImmersiveForFullscreen(false);
  test_surface->SetFullscreen(true, display::kInvalidDisplayId);
  test_surface->surface_for_testing()->Commit();

  EXPECT_TRUE(GetTopLevelWindowState(test_surface)->IsFullscreen());
  EXPECT_TRUE(GetEscNotification(test_surface));
}

TEST_F(UILockControllerTest, EscNotificationClosesAfterDuration) {
  std::unique_ptr<ShellSurface> test_surface = BuildSurface(1024, 768);
  test_surface->SetUseImmersiveForFullscreen(false);
  test_surface->SetFullscreen(true, display::kInvalidDisplayId);
  test_surface->surface_for_testing()->Commit();

  EXPECT_TRUE(GetEscNotification(test_surface));
  task_environment()->FastForwardBy(base::Seconds(5));
  EXPECT_FALSE(GetEscNotification(test_surface));
}

TEST_F(UILockControllerTest, HoldingEscapeHidesNotification) {
  std::unique_ptr<ShellSurface> test_surface = BuildSurface(1024, 768);
  test_surface->SetUseImmersiveForFullscreen(false);
  test_surface->SetFullscreen(true, display::kInvalidDisplayId);
  test_surface->surface_for_testing()->Commit();

  EXPECT_TRUE(GetTopLevelWindowState(test_surface)->IsFullscreen());
  EXPECT_TRUE(GetEscNotification(test_surface));

  GetEventGenerator()->PressKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  task_environment()->FastForwardBy(base::Seconds(3));

  EXPECT_FALSE(GetTopLevelWindowState(test_surface)->IsFullscreen());
  EXPECT_FALSE(GetEscNotification(test_surface));
}

TEST_F(UILockControllerTest, LosingFullscreenHidesNotification) {
  std::unique_ptr<ShellSurface> test_surface = BuildSurface(1024, 768);
  test_surface->SetUseImmersiveForFullscreen(false);
  test_surface->SetFullscreen(true, display::kInvalidDisplayId);
  test_surface->surface_for_testing()->Commit();

  EXPECT_TRUE(GetTopLevelWindowState(test_surface)->IsFullscreen());
  EXPECT_TRUE(GetEscNotification(test_surface));

  // Have surface loose fullscreen, notification should now be hidden.
  test_surface->Minimize();
  test_surface->SetFullscreen(false, display::kInvalidDisplayId);
  test_surface->surface_for_testing()->Commit();

  EXPECT_FALSE(GetTopLevelWindowState(test_surface)->IsFullscreen());
  EXPECT_FALSE(
      seat_->GetUILockControllerForTesting()->GetEscNotificationForTesting(
          GetTopLevelWindow(test_surface)));
}

TEST_F(UILockControllerTest, EscNotificationIsReshownIfInterrupted) {
  std::unique_ptr<ShellSurface> test_surface = BuildSurface(1024, 768);
  test_surface->SetUseImmersiveForFullscreen(false);
  test_surface->SetFullscreen(true, display::kInvalidDisplayId);
  test_surface->surface_for_testing()->Commit();

  EXPECT_TRUE(GetEscNotification(test_surface));

  // Stop fullscreen.
  test_surface->SetFullscreen(false, display::kInvalidDisplayId);
  EXPECT_FALSE(
      seat_->GetUILockControllerForTesting()->GetEscNotificationForTesting(
          GetTopLevelWindow(test_surface)));

  // Fullscreen should show notification since it did not stay visible for
  // duration.
  test_surface->SetFullscreen(true, display::kInvalidDisplayId);
  EXPECT_TRUE(GetEscNotification(test_surface));

  // After duration, notification should be removed.
  task_environment()->FastForwardBy(base::Seconds(5));
  EXPECT_FALSE(GetEscNotification(test_surface));

  // Notification is shown after fullscreen toggle.
  test_surface->SetFullscreen(false, display::kInvalidDisplayId);
  test_surface->SetFullscreen(true, display::kInvalidDisplayId);
  EXPECT_TRUE(GetEscNotification(test_surface));
}

TEST_F(UILockControllerTest, EscNotificationIsReshownAfterUnlock) {
  // Arrange: Go fullscreen and time out the notification.
  std::unique_ptr<ShellSurface> test_surface = BuildSurface(1024, 768);
  test_surface->SetUseImmersiveForFullscreen(false);
  test_surface->SetFullscreen(true, display::kInvalidDisplayId);
  test_surface->surface_for_testing()->Commit();
  task_environment()->FastForwardBy(base::Seconds(10));
  // Ensure the notification did time out; if not, we can't trust the test
  // result.
  EXPECT_FALSE(GetEscNotification(test_surface));

  // Act: Simulate locking and unlocking.
  GetSessionControllerClient()->LockScreen();
  GetSessionControllerClient()->UnlockScreen();

  // Assert: Notification shown again.
  EXPECT_TRUE(GetEscNotification(test_surface));
}

TEST_F(UILockControllerTest, EscNotificationReshownWhenScreenTurnedOn) {
  // Arrange: Set up a pointer capture notification, then let it expire.
  std::unique_ptr<ShellSurface> test_surface = BuildSurface(1024, 768);
  test_surface->SetUseImmersiveForFullscreen(false);
  test_surface->SetFullscreen(true, display::kInvalidDisplayId);
  test_surface->surface_for_testing()->Commit();
  task_environment()->FastForwardBy(base::Seconds(10));
  // Ensure the notification did time out; if not, we can't trust the test
  // result.
  EXPECT_FALSE(GetEscNotification(test_surface));

  // Act: Simulate turning the backlight off and on again.
  power_manager::SetBacklightBrightnessRequest request;
  request.set_percent(0);
  chromeos::FakePowerManagerClient::Get()->SetScreenBrightness(request);
  base::RunLoop().RunUntilIdle();
  request.set_percent(100);
  chromeos::FakePowerManagerClient::Get()->SetScreenBrightness(request);
  base::RunLoop().RunUntilIdle();

  // Assert: Notification shown again.
  EXPECT_TRUE(GetEscNotification(test_surface));
}

TEST_F(UILockControllerTest, EscNotificationReshownWhenLidReopened) {
  // Arrange: Set up a pointer capture notification, then let it expire.
  std::unique_ptr<ShellSurface> test_surface = BuildSurface(1024, 768);
  test_surface->SetUseImmersiveForFullscreen(false);
  test_surface->SetFullscreen(true, display::kInvalidDisplayId);
  test_surface->surface_for_testing()->Commit();
  task_environment()->FastForwardBy(base::Seconds(10));
  // Ensure the notification did time out; if not, we can't trust the test
  // result.
  EXPECT_FALSE(GetEscNotification(test_surface));

  // Act: Simulate closing and reopening the lid.
  chromeos::FakePowerManagerClient::Get()->SetLidState(
      chromeos::PowerManagerClient::LidState::CLOSED, base::TimeTicks::Now());
  chromeos::FakePowerManagerClient::Get()->SetLidState(
      chromeos::PowerManagerClient::LidState::OPEN, base::TimeTicks::Now());

  // Assert: Notification shown again.
  EXPECT_TRUE(GetEscNotification(test_surface));
}

TEST_F(UILockControllerTest, EscNotificationShowsOnSecondaryDisplay) {
  // Create surface on secondary display.
  UpdateDisplay("900x800,70x600");
  std::unique_ptr<ShellSurface> test_surface =
      BuildSurface(gfx::Point(900, 100), 200, 200);
  test_surface->SetUseImmersiveForFullscreen(false);
  test_surface->SetFullscreen(true, display::kInvalidDisplayId);
  test_surface->surface_for_testing()->Commit();

  // Esc notification should be in secondary display.
  views::Widget* esc_notification = GetEscNotification(test_surface);
  EXPECT_TRUE(GetSecondaryDisplay().bounds().Contains(
      esc_notification->GetWindowBoundsInScreen()));
}

TEST_F(UILockControllerTest, PointerLockShowsNotification) {
  std::unique_ptr<ShellSurface> test_surface = BuildSurface(1024, 768);
  test_surface->SetApplicationId(kOverviewToExitAppId);
  test_surface->surface_for_testing()->Commit();
  testing::NiceMock<MockPointerDelegate> delegate;
  Pointer pointer(&delegate, seat_.get());
  testing::NiceMock<MockPointerConstraintDelegate> constraint(
      &pointer, test_surface->surface_for_testing());
  EXPECT_FALSE(GetPointerCaptureNotification(test_surface));

  EXPECT_TRUE(pointer.ConstrainPointer(&constraint));

  EXPECT_TRUE(GetPointerCaptureNotification(test_surface));
}

TEST_F(UILockControllerTest, PointerLockNotificationObeysCooldown) {
  // Arrange: Set up a pointer capture notification.
  std::unique_ptr<ShellSurface> test_surface = BuildSurface(1024, 768);
  test_surface->SetApplicationId(kOverviewToExitAppId);
  test_surface->surface_for_testing()->Commit();
  testing::NiceMock<MockPointerDelegate> delegate;
  Pointer pointer(&delegate, seat_.get());
  testing::NiceMock<MockPointerConstraintDelegate> constraint(
      &pointer, test_surface->surface_for_testing());
  EXPECT_TRUE(pointer.ConstrainPointer(&constraint));
  EXPECT_TRUE(GetPointerCaptureNotification(test_surface));

  // Act: Wait for the notification to timeout.
  task_environment()->FastForwardBy(base::Seconds(5));

  // Assert: Notification has disappeared.
  EXPECT_FALSE(GetPointerCaptureNotification(test_surface));

  // Act: Remove and re-apply the constraint.
  pointer.OnPointerConstraintDelegateDestroying(&constraint);
  EXPECT_TRUE(pointer.ConstrainPointer(&constraint));

  // Assert: Notification not shown due to the cooldown.
  EXPECT_FALSE(GetPointerCaptureNotification(test_surface));

  // Act: Wait for the cooldown, then re-apply again
  pointer.OnPointerConstraintDelegateDestroying(&constraint);
  task_environment()->FastForwardBy(base::Minutes(5));
  EXPECT_TRUE(pointer.ConstrainPointer(&constraint));

  // Assert: Cooldown has expired so notification is shown.
  EXPECT_TRUE(GetPointerCaptureNotification(test_surface));
}

TEST_F(UILockControllerTest, PointerLockNotificationReshownOnLidOpen) {
  // Arrange: Set up a pointer capture notification, then let it expire.
  std::unique_ptr<ShellSurface> test_surface = BuildSurface(1024, 768);
  test_surface->SetApplicationId(kOverviewToExitAppId);
  test_surface->surface_for_testing()->Commit();
  testing::NiceMock<MockPointerDelegate> delegate;
  Pointer pointer(&delegate, seat_.get());
  testing::NiceMock<MockPointerConstraintDelegate> constraint(
      &pointer, test_surface->surface_for_testing());
  EXPECT_TRUE(pointer.ConstrainPointer(&constraint));
  EXPECT_TRUE(GetPointerCaptureNotification(test_surface));
  task_environment()->FastForwardBy(base::Seconds(5));
  EXPECT_FALSE(GetPointerCaptureNotification(test_surface));

  // Act: Simulate closing and reopening the lid.
  chromeos::FakePowerManagerClient::Get()->SetLidState(
      chromeos::PowerManagerClient::LidState::CLOSED, base::TimeTicks::Now());
  chromeos::FakePowerManagerClient::Get()->SetLidState(
      chromeos::PowerManagerClient::LidState::OPEN, base::TimeTicks::Now());

  // Assert: Notification shown again.
  EXPECT_TRUE(GetPointerCaptureNotification(test_surface));
}

TEST_F(UILockControllerTest, PointerLockNotificationReshownWhenScreenTurnedOn) {
  // Arrange: Set up a pointer capture notification, then let it expire.
  std::unique_ptr<ShellSurface> test_surface = BuildSurface(1024, 768);
  test_surface->SetApplicationId(kOverviewToExitAppId);
  test_surface->surface_for_testing()->Commit();
  testing::NiceMock<MockPointerDelegate> delegate;
  Pointer pointer(&delegate, seat_.get());
  testing::NiceMock<MockPointerConstraintDelegate> constraint(
      &pointer, test_surface->surface_for_testing());
  EXPECT_TRUE(pointer.ConstrainPointer(&constraint));
  EXPECT_TRUE(GetPointerCaptureNotification(test_surface));
  task_environment()->FastForwardBy(base::Seconds(5));
  EXPECT_FALSE(GetPointerCaptureNotification(test_surface));

  // Act: Simulate turning the backlight off and on again.
  power_manager::SetBacklightBrightnessRequest request;
  request.set_percent(0);
  chromeos::FakePowerManagerClient::Get()->SetScreenBrightness(request);
  base::RunLoop().RunUntilIdle();
  request.set_percent(100);
  chromeos::FakePowerManagerClient::Get()->SetScreenBrightness(request);
  base::RunLoop().RunUntilIdle();

  // Assert: Notification shown again.
  EXPECT_TRUE(GetPointerCaptureNotification(test_surface));
}

TEST_F(UILockControllerTest, PointerLockNotificationReshownOnUnlock) {
  // Lock screen takes focus and it disables pointer capture.
  GetSessionControllerClient()->set_show_lock_screen_views(false);

  // Arrange: Set up a pointer capture notification, then let it expire.
  std::unique_ptr<ShellSurface> test_surface = BuildSurface(1024, 768);
  test_surface->SetApplicationId(kOverviewToExitAppId);
  test_surface->surface_for_testing()->Commit();
  testing::NiceMock<MockPointerDelegate> delegate;
  Pointer pointer(&delegate, seat_.get());
  testing::NiceMock<MockPointerConstraintDelegate> constraint(
      &pointer, test_surface->surface_for_testing());
  EXPECT_TRUE(pointer.ConstrainPointer(&constraint));
  task_environment()->FastForwardBy(base::Seconds(5));
  EXPECT_FALSE(GetPointerCaptureNotification(test_surface));

  // Act: Simulate locking and unlocking.
  GetSessionControllerClient()->LockScreen();
  GetSessionControllerClient()->UnlockScreen();

  // Assert: Notification shown again.
  EXPECT_TRUE(GetPointerCaptureNotification(test_surface));
}

TEST_F(UILockControllerTest, PointerLockNotificationReshownAfterSuspend) {
  // Arrange: Set up a pointer capture notification, then let it expire.
  std::unique_ptr<ShellSurface> test_surface = BuildSurface(1024, 768);
  test_surface->SetApplicationId(kOverviewToExitAppId);
  test_surface->surface_for_testing()->Commit();
  testing::NiceMock<MockPointerDelegate> delegate;
  Pointer pointer(&delegate, seat_.get());
  testing::NiceMock<MockPointerConstraintDelegate> constraint(
      &pointer, test_surface->surface_for_testing());
  EXPECT_TRUE(pointer.ConstrainPointer(&constraint));
  EXPECT_TRUE(GetPointerCaptureNotification(test_surface));
  task_environment()->FastForwardBy(base::Seconds(5));
  EXPECT_FALSE(GetPointerCaptureNotification(test_surface));

  // Act: Simulate suspend and resume
  chromeos::FakePowerManagerClient::Get()->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_IDLE);
  task_environment()->FastForwardBy(base::Minutes(1));
  chromeos::FakePowerManagerClient::Get()->SendSuspendDone(base::Minutes(1));

  // Assert: Notification shown again.
  EXPECT_TRUE(GetPointerCaptureNotification(test_surface));
}

TEST_F(UILockControllerTest, PointerLockNotificationReshownAfterIdle) {
  // Arrange: Set up a pointer capture notification, then let it expire.
  std::unique_ptr<ShellSurface> test_surface = BuildSurface(1024, 768);
  test_surface->SetApplicationId(kOverviewToExitAppId);
  test_surface->surface_for_testing()->Commit();
  testing::NiceMock<MockPointerDelegate> delegate;
  Pointer pointer(&delegate, seat_.get());
  testing::NiceMock<MockPointerConstraintDelegate> constraint(
      &pointer, test_surface->surface_for_testing());
  EXPECT_TRUE(pointer.ConstrainPointer(&constraint));
  EXPECT_TRUE(GetPointerCaptureNotification(test_surface));
  task_environment()->FastForwardBy(base::Seconds(5));
  EXPECT_FALSE(GetPointerCaptureNotification(test_surface));

  // Act: Simulate activity, then go idle.
  seat_->GetUILockControllerForTesting()->OnUserActivity(/*event=*/nullptr);
  task_environment()->FastForwardBy(base::Minutes(10));

  // Assert: Notification not yet shown again.
  EXPECT_FALSE(GetPointerCaptureNotification(test_surface));

  // Act: Simulate activity after being idle.
  seat_->GetUILockControllerForTesting()->OnUserActivity(/*event=*/nullptr);

  // Assert: Notification shown again.
  EXPECT_TRUE(GetPointerCaptureNotification(test_surface));
}

TEST_F(UILockControllerTest, PointerLockCooldownResetForAllWindows) {
  // Arrange: Create two surfaces, one with a pointer lock notification.
  std::unique_ptr<ShellSurface> other_surface = BuildSurface(1024, 768);
  other_surface->SetApplicationId(kOverviewToExitAppId);
  other_surface->surface_for_testing()->Commit();

  std::unique_ptr<ShellSurface> test_surface = BuildSurface(1024, 768);
  test_surface->SetApplicationId(kOverviewToExitAppId);
  test_surface->surface_for_testing()->Commit();

  testing::NiceMock<MockPointerDelegate> delegate;
  Pointer pointer(&delegate, seat_.get());
  testing::NiceMock<MockPointerConstraintDelegate> constraint(
      &pointer, test_surface->surface_for_testing());
  EXPECT_TRUE(pointer.ConstrainPointer(&constraint));
  EXPECT_TRUE(GetPointerCaptureNotification(test_surface));

  // Act: Focus the other window, then lock and unlock.
  wm::ActivateWindow(other_surface->surface_for_testing()->window());
  GetSessionControllerClient()->LockScreen();
  GetSessionControllerClient()->UnlockScreen();

  // Assert: Notification shown again.
  EXPECT_TRUE(GetPointerCaptureNotification(test_surface));
}

TEST_F(UILockControllerTest, FullscreenNotificationHasPriority) {
  // Arrange: Set up a pointer capture notification.
  std::unique_ptr<ShellSurface> test_surface = BuildSurface(1024, 768);
  test_surface->SetApplicationId(kOverviewToExitAppId);
  test_surface->surface_for_testing()->Commit();
  testing::NiceMock<MockPointerDelegate> delegate;
  Pointer pointer(&delegate, seat_.get());
  testing::NiceMock<MockPointerConstraintDelegate> constraint(
      &pointer, test_surface->surface_for_testing());
  EXPECT_TRUE(pointer.ConstrainPointer(&constraint));
  EXPECT_TRUE(GetPointerCaptureNotification(test_surface));

  // Act: Go fullscreen.
  test_surface->SetUseImmersiveForFullscreen(false);
  test_surface->SetFullscreen(true, display::kInvalidDisplayId);
  test_surface->surface_for_testing()->Commit();

  // Assert: Fullscreen notification overrides pointer notification.
  EXPECT_FALSE(GetPointerCaptureNotification(test_surface));
  EXPECT_TRUE(GetEscNotification(test_surface));

  // Act: Exit fullscreen.
  test_surface->SetFullscreen(false, display::kInvalidDisplayId);
  test_surface->surface_for_testing()->Commit();

  // Assert: Pointer notification returns, since it was interrupted.
  EXPECT_TRUE(GetPointerCaptureNotification(test_surface));
  EXPECT_FALSE(GetEscNotification(test_surface));
}

TEST_F(UILockControllerTest, ExitPopup) {
  std::unique_ptr<ShellSurface> test_surface = BuildSurface(1024, 768);
  test_surface->SetUseImmersiveForFullscreen(false);
  test_surface->SetFullscreen(true, display::kInvalidDisplayId);
  test_surface->surface_for_testing()->Commit();
  auto* window_state = GetTopLevelWindowState(test_surface);
  EXPECT_TRUE(window_state->IsFullscreen());
  aura::Window* window = GetTopLevelWindow(test_surface);
  EXPECT_FALSE(IsExitPopupVisible(window));
  EXPECT_TRUE(GetEscNotification(test_surface));

  // Move mouse above y=3 should not show exit popup while notification is
  // visible.
  GetEventGenerator()->MoveMouseTo(0, 2);
  EXPECT_FALSE(IsExitPopupVisible(window));

  // Wait for notification to close, now exit popup should show.
  task_environment()->FastForwardBy(base::Seconds(5));
  EXPECT_FALSE(GetEscNotification(test_surface));
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
  std::unique_ptr<ShellSurface> test_surface = BuildSurface(1024, 768);
  // Set chromeos::kUseOverviewToExitFullscreen on TopLevelWindow.
  test_surface->SetApplicationId(kOverviewToExitAppId);
  test_surface->SetUseImmersiveForFullscreen(false);
  test_surface->SetFullscreen(true, display::kInvalidDisplayId);
  test_surface->surface_for_testing()->Commit();
  EXPECT_FALSE(IsExitPopupVisible(GetTopLevelWindow(test_surface)));

  // Move mouse above y=3 should not show exit popup.
  GetEventGenerator()->MoveMouseTo(0, 2);
  EXPECT_FALSE(IsExitPopupVisible(GetTopLevelWindow(test_surface)));
}

TEST_F(UILockControllerTest, OnlyShowWhenActive) {
  std::unique_ptr<ShellSurface> test_surface1 = BuildSurface(1024, 768);
  test_surface1->surface_for_testing()->Commit();
  std::unique_ptr<ShellSurface> test_surface2 =
      BuildSurface(gfx::Point(100, 100), 200, 200);
  test_surface2->surface_for_testing()->Commit();

  // Surface2 is active when we make Surface1 fullscreen.
  // Esc notification, and exit popup should not be shown.
  test_surface1->SetFullscreen(true, display::kInvalidDisplayId);
  EXPECT_FALSE(GetEscNotification(test_surface1));
  GetEventGenerator()->MoveMouseTo(0, 2);
  EXPECT_FALSE(IsExitPopupVisible(GetTopLevelWindow(test_surface1)));
}

}  // namespace
}  // namespace exo
