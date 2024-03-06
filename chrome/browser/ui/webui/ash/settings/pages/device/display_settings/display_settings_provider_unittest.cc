// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/device/display_settings/display_settings_provider.h"

#include "ash/display/display_performance_mode_controller.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/shell.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/screen.h"

namespace ash::settings {

namespace {

// A mock observer that records current tablet mode status and counts when
// OnTabletModeChanged function is called.
class FakeTabletModeObserver : public mojom::TabletModeObserver {
 public:
  uint32_t num_tablet_mode_change_calls() const {
    return num_tablet_mode_change_calls_;
  }

  bool is_tablet_mode() { return is_tablet_mode_; }

  // mojom::TabletModeObserver:
  void OnTabletModeChanged(bool is_tablet_mode) override {
    ++num_tablet_mode_change_calls_;
    is_tablet_mode_ = is_tablet_mode;

    if (quit_callback_) {
      std::move(quit_callback_).Run();
    }
  }

  void WaitForTabletModeChanged() {
    DCHECK(quit_callback_.is_null());
    base::RunLoop loop;
    quit_callback_ = loop.QuitClosure();
    loop.Run();
  }

  mojo::Receiver<mojom::TabletModeObserver> receiver{this};

 private:
  uint32_t num_tablet_mode_change_calls_ = 0;
  bool is_tablet_mode_ = false;
  base::OnceClosure quit_callback_;
};

// A mock observer that counts when ObserveDisplayConfiguration function is
// called.
class FakeDisplayConfigurationObserver
    : public mojom::DisplayConfigurationObserver {
 public:
  uint32_t num_display_configuration_changed_calls() const {
    return num_display_configuration_changed_calls_;
  }

  // mojom::DisplayConfigurationObserver:
  void OnDisplayConfigurationChanged() override {
    ++num_display_configuration_changed_calls_;

    if (quit_callback_) {
      std::move(quit_callback_).Run();
    }
  }

  void WaitForDisplayConfigurationChanged() {
    DCHECK(quit_callback_.is_null());
    base::RunLoop loop;
    quit_callback_ = loop.QuitClosure();
    loop.Run();
  }

  mojo::Receiver<mojom::DisplayConfigurationObserver> receiver{this};

 private:
  uint32_t num_display_configuration_changed_calls_ = 0;
  base::OnceClosure quit_callback_;
};

}  // namespace

class DisplaySettingsProviderTest : public ChromeAshTestBase {
 public:
  DisplaySettingsProviderTest()
      : ChromeAshTestBase(std::make_unique<content::BrowserTaskEnvironment>(
            content::BrowserTaskEnvironment::TimeSource::MOCK_TIME)) {}
  ~DisplaySettingsProviderTest() override = default;

  void SetUp() override {
    ChromeAshTestBase::SetUp();
    provider_ = std::make_unique<DisplaySettingsProvider>();
  }

  void TearDown() override {
    provider_.reset();
    ChromeAshTestBase::TearDown();
  }

  void FastForwardBy(base::TimeDelta delta) {
    task_environment()->FastForwardBy(delta);
  }

 protected:
  std::unique_ptr<DisplaySettingsProvider> provider_;
  base::HistogramTester histogram_tester_;
};

// Test the behavior when the tablet mode status has changed. The tablet mode is
// initialized as "not-in-tablet-mode".
TEST_F(DisplaySettingsProviderTest, TabletModeObservation) {
  FakeTabletModeObserver fake_observer;
  base::test::TestFuture<bool> future;

  // Attach a tablet mode observer.
  provider_->ObserveTabletMode(
      fake_observer.receiver.BindNewPipeAndPassRemote(), future.GetCallback());
  base::RunLoop().RunUntilIdle();

  // Default initial state is "not-in-tablet-mode".
  ASSERT_FALSE(future.Get<0>());

  provider_->OnTabletModeEventsBlockingChanged();
  fake_observer.WaitForTabletModeChanged();

  EXPECT_EQ(1u, fake_observer.num_tablet_mode_change_calls());
}

// Test the behavior when the display configuration has changed.
TEST_F(DisplaySettingsProviderTest, DisplayConfigurationObservation) {
  FakeDisplayConfigurationObserver fake_observer;

  // Attach a display configuration observer.
  provider_->ObserveDisplayConfiguration(
      fake_observer.receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  provider_->OnDidProcessDisplayChanges(/*configuration_change=*/{{}, {}, {}});
  fake_observer.WaitForDisplayConfigurationChanged();

  EXPECT_EQ(1u, fake_observer.num_display_configuration_changed_calls());
}

// Test histogram is recorded when users change display settings.
TEST_F(DisplaySettingsProviderTest, ChangeDisplaySettingsHistogram) {
  // Loop through all display setting types.
  for (int typeInt = static_cast<int>(mojom::DisplaySettingsType::kMinValue);
       typeInt <= static_cast<int>(mojom::DisplaySettingsType::kMaxValue);
       typeInt++) {
    mojom::DisplaySettingsType type =
        static_cast<mojom::DisplaySettingsType>(typeInt);
    // Settings applied to both internal and external displays.
    if (type == mojom::DisplaySettingsType::kDisplayPage ||
        type == mojom::DisplaySettingsType::kMirrorMode ||
        type == mojom::DisplaySettingsType::kUnifiedMode ||
        type == mojom::DisplaySettingsType::kPrimaryDisplay) {
      auto value = mojom::DisplaySettingsValue::New();
      if (type == mojom::DisplaySettingsType::kMirrorMode) {
        value->mirror_mode_status = true;
      } else if (type == mojom::DisplaySettingsType::kUnifiedMode) {
        value->unified_mode_status = true;
      }
      provider_->RecordChangingDisplaySettings(type, std::move(value));
      histogram_tester_.ExpectBucketCount(
          DisplaySettingsProvider::kDisplaySettingsHistogramName, type, 1);
    } else {
      // Settings applied to either internal or external displays.
      for (bool internal : {true, false}) {
        auto value = mojom::DisplaySettingsValue::New();
        value->is_internal_display = internal;
        if (type == mojom::DisplaySettingsType::kOrientation) {
          value->orientation =
              mojom::DisplaySettingsOrientationOption::k90Degree;
        } else if (type == mojom::DisplaySettingsType::kNightLight) {
          value->night_light_status = true;
        } else if (type == mojom::DisplaySettingsType::kNightLightSchedule) {
          value->night_light_schedule =
              mojom::DisplaySettingsNightLightScheduleOption::kSunsetToSunrise;
        }
        provider_->RecordChangingDisplaySettings(type, std::move(value));

        std::string histogram_name(
            DisplaySettingsProvider::kDisplaySettingsHistogramName);
        histogram_name.append(internal ? ".Internal" : ".External");
        histogram_tester_.ExpectBucketCount(histogram_name, type, 1);
      }
    }
  }
}

// Test histogram is recorded when users change display orientation.
TEST_F(DisplaySettingsProviderTest, ChangeDisplayOrientationHistogram) {
  for (int orientation_int =
           static_cast<int>(mojom::DisplaySettingsOrientationOption::kMinValue);
       orientation_int <=
       static_cast<int>(mojom::DisplaySettingsOrientationOption::kMaxValue);
       orientation_int++) {
    mojom::DisplaySettingsOrientationOption orientation =
        static_cast<mojom::DisplaySettingsOrientationOption>(orientation_int);
    // Settings applied to either internal or external displays.
    for (bool internal : {true, false}) {
      auto value = mojom::DisplaySettingsValue::New();
      value->is_internal_display = internal;
      value->orientation = orientation;
      provider_->RecordChangingDisplaySettings(
          mojom::DisplaySettingsType::kOrientation, std::move(value));

      std::string histogram_name(
          DisplaySettingsProvider::kDisplaySettingsHistogramName);
      histogram_name.append(internal ? ".Internal" : ".External");
      histogram_name.append(".Orientation");
      histogram_tester_.ExpectBucketCount(histogram_name, orientation, 1);
    }
  }
}

// Test histogram is recorded when users toggle display night light status.
TEST_F(DisplaySettingsProviderTest, ToggleDisplayNightLightStatusHistogram) {
  for (bool night_light_status : {true, false}) {
    // Settings applied to either internal or external displays.
    for (bool internal : {true, false}) {
      auto value = mojom::DisplaySettingsValue::New();
      value->is_internal_display = internal;
      value->night_light_status = night_light_status;
      provider_->RecordChangingDisplaySettings(
          mojom::DisplaySettingsType::kNightLight, std::move(value));

      std::string histogram_name(
          DisplaySettingsProvider::kDisplaySettingsHistogramName);
      histogram_name.append(internal ? ".Internal" : ".External");
      histogram_name.append(".NightLightStatus");
      histogram_tester_.ExpectBucketCount(histogram_name, night_light_status,
                                          1);
    }
  }
}

// Test histogram is recorded when users change display night light schedule.
TEST_F(DisplaySettingsProviderTest, ToggleDisplayNightLightScheduleHistogram) {
  for (int night_light_schedule_int = static_cast<int>(
           mojom::DisplaySettingsNightLightScheduleOption::kMinValue);
       night_light_schedule_int <=
       static_cast<int>(
           mojom::DisplaySettingsNightLightScheduleOption::kMaxValue);
       night_light_schedule_int++) {
    mojom::DisplaySettingsNightLightScheduleOption night_light_schedule =
        static_cast<mojom::DisplaySettingsNightLightScheduleOption>(
            night_light_schedule_int);
    // Settings applied to either internal or external displays.
    for (bool internal : {true, false}) {
      auto value = mojom::DisplaySettingsValue::New();
      value->is_internal_display = internal;
      value->night_light_schedule = night_light_schedule;
      provider_->RecordChangingDisplaySettings(
          mojom::DisplaySettingsType::kNightLightSchedule, std::move(value));

      std::string histogram_name(
          DisplaySettingsProvider::kDisplaySettingsHistogramName);
      histogram_name.append(internal ? ".Internal" : ".External");
      histogram_name.append(".NightLightSchedule");
      histogram_tester_.ExpectBucketCount(histogram_name, night_light_schedule,
                                          1);
    }
  }
}

// Test histogram is recorded when users toggle display mirror mode status.
TEST_F(DisplaySettingsProviderTest, ToggleDisplayMirrorModeStatusHistogram) {
  for (bool mirror_mode_status : {true, false}) {
    auto value = mojom::DisplaySettingsValue::New();
    value->mirror_mode_status = mirror_mode_status;
    provider_->RecordChangingDisplaySettings(
        mojom::DisplaySettingsType::kMirrorMode, std::move(value));

    std::string histogram_name(
        DisplaySettingsProvider::kDisplaySettingsHistogramName);
    histogram_name.append(".MirrorModeStatus");
    histogram_tester_.ExpectBucketCount(histogram_name, mirror_mode_status, 1);
  }
}

// Test histogram is recorded when users toggle display unified mode status.
TEST_F(DisplaySettingsProviderTest, ToggleDisplayUnifiedModeStatusHistogram) {
  for (bool unified_mode_status : {true, false}) {
    auto value = mojom::DisplaySettingsValue::New();
    value->unified_mode_status = unified_mode_status;
    provider_->RecordChangingDisplaySettings(
        mojom::DisplaySettingsType::kUnifiedMode, std::move(value));

    std::string histogram_name(
        DisplaySettingsProvider::kDisplaySettingsHistogramName);
    histogram_name.append(".UnifiedModeStatus");
    histogram_tester_.ExpectBucketCount(histogram_name, unified_mode_status, 1);
  }
}

// Test histogram is recorded only when a display is connected for the first
// time.
TEST_F(DisplaySettingsProviderTest, NewDisplayConnectedHistogram) {
  int64_t id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  provider_->OnDisplayAdded(display::Display(id));

  // Expect to count new display is connected.
  histogram_tester_.ExpectBucketCount(
      DisplaySettingsProvider::kNewDisplayConnectedHistogram,
      DisplaySettingsProvider::DisplayType::kExternalDisplay, 1);

  UpdateDisplay("300x200");
  provider_->OnDisplayAdded(display::Display(id));

  // Expect not to count new display is connected since it's already saved
  // into prefs before.
  histogram_tester_.ExpectBucketCount(
      DisplaySettingsProvider::kNewDisplayConnectedHistogram,
      DisplaySettingsProvider::DisplayType::kExternalDisplay, 1);

  // Entering unified desk mode should not count new display connected.
  provider_->OnDisplayAdded(display::Display(display::kUnifiedDisplayId));
  histogram_tester_.ExpectBucketCount(
      DisplaySettingsProvider::kNewDisplayConnectedHistogram,
      DisplaySettingsProvider::DisplayType::kExternalDisplay, 1);
}

// Test histogram is recorded when user overrides system default display
// settings.
TEST_F(DisplaySettingsProviderTest, UserOverrideDefaultSettingsHistogram) {
  int64_t id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  provider_->OnDisplayAdded(display::Display(id));

  constexpr uint16_t kTimeDeltaInMinute = 15;
  FastForwardBy(base::Minutes(kTimeDeltaInMinute));

  auto value = mojom::DisplaySettingsValue::New();
  value->is_internal_display = false;
  value->display_id = id;
  provider_->RecordChangingDisplaySettings(
      mojom::DisplaySettingsType::kResolution, std::move(value));

  histogram_tester_.ExpectTimeBucketCount(
      "ChromeOS.Settings.Display.External."
      "UserOverrideDisplayDefaultSettingsTimeElapsed.Resolution",
      base::Minutes(kTimeDeltaInMinute) / base::Minutes(1).InMilliseconds(),
      /*expected_count=*/1);
}

TEST_F(DisplaySettingsProviderTest, UserToggleDisplayPerformance) {
  provider_->SetShinyPerformance(true);
  EXPECT_EQ(Shell::Get()
                ->display_performance_mode_controller()
                ->GetCurrentStateForTesting(),
            DisplayPerformanceModeController::ModeState::kHighPerformance);

  provider_->SetShinyPerformance(false);
  EXPECT_NE(Shell::Get()
                ->display_performance_mode_controller()
                ->GetCurrentStateForTesting(),
            DisplayPerformanceModeController::ModeState::kHighPerformance);
}

}  // namespace ash::settings
