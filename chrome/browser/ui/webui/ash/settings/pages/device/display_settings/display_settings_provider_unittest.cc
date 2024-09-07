// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/device/display_settings/display_settings_provider.h"

#include "ash/constants/ash_features.h"
#include "ash/display/display_performance_mode_controller.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/shell.h"
#include "ash/system/brightness/brightness_controller_chromeos.h"
#include "ash/system/brightness_control_delegate.h"
#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/screen.h"

namespace ash::settings {

namespace {

constexpr base::TimeDelta kMetricsDelayTimerInterval = base::Seconds(2);

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

// A mock observer that counts when OnDisplayBrightnessChanged function is
// called.
class FakeDisplayBrightnessSettingsObserver
    : public mojom::DisplayBrightnessSettingsObserver {
 public:
  uint32_t num_display_brightness_changed_calls() const {
    return num_display_brightness_changed_calls_;
  }

  double current_brightness() { return current_brightness_; }

  // mojom::DisplayBrightnessSettingsObserver:
  void OnDisplayBrightnessChanged(double brightness_percent,
                                  bool triggered_by_als) override {
    ++num_display_brightness_changed_calls_;
    current_brightness_ = brightness_percent;

    if (quit_callback_) {
      std::move(quit_callback_).Run();
    }
  }

  void WaitForDisplayBrightnessChanged() {
    DCHECK(quit_callback_.is_null());
    base::RunLoop loop;
    quit_callback_ = loop.QuitClosure();
    loop.Run();
  }

  mojo::Receiver<mojom::DisplayBrightnessSettingsObserver> receiver{this};

 private:
  uint32_t num_display_brightness_changed_calls_ = 0;
  double current_brightness_ = 0;
  base::OnceClosure quit_callback_;
};

// A mock observer that counts when OnAmbientLightSensorEnabledChanged function
// is called.
class FakeAmbientLightSensorObserver
    : public mojom::AmbientLightSensorObserver {
 public:
  uint32_t num_ambient_light_sensor_enabled_changed_calls() const {
    return num_ambient_light_sensor_enabled_changed_calls_;
  }

  double is_ambient_light_sensor_enabled() {
    return is_ambient_light_sensor_enabled_;
  }

  // mojom::AmbientLightSensorObserver:
  void OnAmbientLightSensorEnabledChanged(
      bool is_ambient_light_sensor_enabled) override {
    ++num_ambient_light_sensor_enabled_changed_calls_;
    is_ambient_light_sensor_enabled_ = is_ambient_light_sensor_enabled;

    if (quit_callback_) {
      std::move(quit_callback_).Run();
    }
  }

  void WaitForAmbientLightSensorEnabledChanged() {
    DCHECK(quit_callback_.is_null());
    base::RunLoop loop;
    quit_callback_ = loop.QuitClosure();
    loop.Run();
  }

  mojo::Receiver<mojom::AmbientLightSensorObserver> receiver{this};

 private:
  uint32_t num_ambient_light_sensor_enabled_changed_calls_ = 0;
  bool is_ambient_light_sensor_enabled_ = true;
  base::OnceClosure quit_callback_;
};

class FakeBrightnessControlDelegate : public BrightnessControlDelegate {
 public:
  FakeBrightnessControlDelegate() = default;

  FakeBrightnessControlDelegate(const FakeBrightnessControlDelegate&) = delete;
  FakeBrightnessControlDelegate& operator=(
      const FakeBrightnessControlDelegate&) = delete;

  ~FakeBrightnessControlDelegate() override = default;

  void HandleBrightnessDown() override {}
  void HandleBrightnessUp() override {}
  void SetBrightnessPercent(double percent,
                            bool gradual,
                            BrightnessChangeSource source) override {
    brightness_percent_ = percent;
    last_brightness_change_source_ = source;
  }
  void GetBrightnessPercent(
      base::OnceCallback<void(std::optional<double>)> callback) override {
    std::move(callback).Run(brightness_percent_);
  }
  void SetAmbientLightSensorEnabled(
      bool enabled,
      BrightnessControlDelegate::AmbientLightSensorEnabledChangeSource source)
      override {
    is_ambient_light_sensor_enabled_ = enabled;
  }
  void GetAmbientLightSensorEnabled(
      base::OnceCallback<void(std::optional<bool>)> callback) override {
    std::move(callback).Run(is_ambient_light_sensor_enabled_);
  }
  void HasAmbientLightSensor(
      base::OnceCallback<void(std::optional<bool>)> callback) override {
    std::move(callback).Run(has_ambient_light_sensor_);
  }

  double brightness_percent() const { return brightness_percent_; }
  BrightnessChangeSource last_brightness_change_source() const {
    return last_brightness_change_source_;
  }
  bool is_ambient_light_sensor_enabled() const {
    return is_ambient_light_sensor_enabled_;
  }
  void set_has_ambient_light_sensor(bool has_ambient_light_sensor) {
    has_ambient_light_sensor_ = has_ambient_light_sensor;
  }

 private:
  double brightness_percent_;
  BrightnessChangeSource last_brightness_change_source_ =
      BrightnessChangeSource::kUnknown;
  // Enabled by default to match system behavior.
  bool is_ambient_light_sensor_enabled_ = true;
  bool has_ambient_light_sensor_ = true;
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
    feature_list_.InitAndDisableFeature(
        features::kEnableBrightnessControlInSettings);
    provider_ = std::make_unique<DisplaySettingsProvider>();
    brightness_control_delegate_ =
        std::make_unique<FakeBrightnessControlDelegate>();
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
  std::unique_ptr<FakeBrightnessControlDelegate> brightness_control_delegate_;
  base::HistogramTester histogram_tester_;
  base::test::ScopedFeatureList feature_list_;
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
  // Expect no metrics fired before new display added.
  histogram_tester_.ExpectBucketCount(
      DisplaySettingsProvider::
          kUserOverrideExternalDisplayDefaultSettingsHistogram,
      DisplaySettingsProvider::DisplayDefaultSettingsMeasurement::
          kNewDisplayConnected,
      0);

  int64_t id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  provider_->OnDisplayAdded(display::Display(id));

  // Expect to count new display is connected.
  histogram_tester_.ExpectBucketCount(
      DisplaySettingsProvider::kNewDisplayConnectedHistogram,
      DisplaySettingsProvider::DisplayType::kExternalDisplay, 1);
  histogram_tester_.ExpectBucketCount(
      DisplaySettingsProvider::
          kUserOverrideExternalDisplayDefaultSettingsHistogram,
      DisplaySettingsProvider::DisplayDefaultSettingsMeasurement::
          kNewDisplayConnected,
      1);

  UpdateDisplay("300x200");
  provider_->OnDisplayAdded(display::Display(id));

  // Expect not to count new display is connected since it's already saved
  // into prefs before.
  histogram_tester_.ExpectBucketCount(
      DisplaySettingsProvider::kNewDisplayConnectedHistogram,
      DisplaySettingsProvider::DisplayType::kExternalDisplay, 1);
  histogram_tester_.ExpectBucketCount(
      DisplaySettingsProvider::
          kUserOverrideExternalDisplayDefaultSettingsHistogram,
      DisplaySettingsProvider::DisplayDefaultSettingsMeasurement::
          kNewDisplayConnected,
      1);

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

  // Expect user override resolution metrics fired.
  histogram_tester_.ExpectBucketCount(
      DisplaySettingsProvider::
          kUserOverrideExternalDisplayDefaultSettingsHistogram,
      DisplaySettingsProvider::DisplayDefaultSettingsMeasurement::
          kOverrideResolution,
      1);

  // Changing resolution again and expect not fire user override resolution
  // metrics.
  value = mojom::DisplaySettingsValue::New();
  value->is_internal_display = false;
  value->display_id = id;
  provider_->RecordChangingDisplaySettings(
      mojom::DisplaySettingsType::kResolution, std::move(value));

  histogram_tester_.ExpectBucketCount(
      DisplaySettingsProvider::
          kUserOverrideExternalDisplayDefaultSettingsHistogram,
      DisplaySettingsProvider::DisplayDefaultSettingsMeasurement::
          kOverrideResolution,
      1);
}

// Test histogram is not recorded when user overrides system default display
// settings after 60 minutes.
TEST_F(DisplaySettingsProviderTest,
       UserOverrideDefaultSettingsHistogramNotFired) {
  int64_t id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  provider_->OnDisplayAdded(display::Display(id));

  constexpr uint16_t kTimeDeltaInMinute = 61;
  FastForwardBy(base::Minutes(kTimeDeltaInMinute));

  auto value = mojom::DisplaySettingsValue::New();
  value->is_internal_display = false;
  value->display_id = id;
  provider_->RecordChangingDisplaySettings(
      mojom::DisplaySettingsType::kResolution, std::move(value));

  // Expect user override resolution metrics not fired.
  histogram_tester_.ExpectBucketCount(
      DisplaySettingsProvider::
          kUserOverrideExternalDisplayDefaultSettingsHistogram,
      DisplaySettingsProvider::DisplayDefaultSettingsMeasurement::
          kOverrideResolution,
      0);
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

// Test the behavior when the display brightness is changed.
TEST_F(DisplaySettingsProviderTest, DisplayBrightnessSettingsObservation) {
  FakeDisplayBrightnessSettingsObserver fake_observer;
  base::test::TestFuture<double> future;

  // Attach a brightness settings observer.
  provider_->ObserveDisplayBrightnessSettings(
      fake_observer.receiver.BindNewPipeAndPassRemote(), future.GetCallback());
  base::RunLoop().RunUntilIdle();

  // Observer should not have been called yet.
  EXPECT_EQ(0u, fake_observer.num_display_brightness_changed_calls());

  double brightness_percent = 55.5;
  power_manager::BacklightBrightnessChange brightness_change;
  brightness_change.set_percent(brightness_percent);
  brightness_change.set_cause(
      power_manager::BacklightBrightnessChange_Cause_USER_REQUEST);
  provider_->ScreenBrightnessChanged(brightness_change);

  fake_observer.WaitForDisplayBrightnessChanged();

  // Observer should have been called.
  EXPECT_EQ(1u, fake_observer.num_display_brightness_changed_calls());
  // The brightness value that the observer received should match the brightness
  // from the provider's observer.
  EXPECT_EQ(brightness_percent, fake_observer.current_brightness());
}

// Test the behavior when setting the internal display screen brightness (when
// the feature flag is disabled).
TEST_F(DisplaySettingsProviderTest,
       SetInternalDisplayScreenBrightness_FeatureDisabled) {
  // No histograms should have been recorded yet.
  histogram_tester_.ExpectTotalCount(
      "ChromeOS.Settings.Display.Internal.BrightnessSliderAdjusted",
      /*expected_count=*/0);

  // Set the brightness with a sentinel value, so we can test that the
  // brightness doesn't change if the feature flag is disabled.
  double brightness_before_setting = 50.0;
  brightness_control_delegate_->SetBrightnessPercent(
      brightness_before_setting,
      /*gradual=*/false, /*source=*/
      BrightnessControlDelegate::BrightnessChangeSource::kQuickSettings);

  provider_->SetBrightnessControlDelegateForTesting(
      brightness_control_delegate_.get());

  double new_brightness_percent = 33.3;
  provider_->SetInternalDisplayScreenBrightness(new_brightness_percent);

  // When feature flag is disabled, setting the brightness has no effect.
  EXPECT_EQ(brightness_before_setting,
            brightness_control_delegate_->brightness_percent());

  // No histograms should have been recorded, because the feature is disabled.
  histogram_tester_.ExpectTotalCount(
      "ChromeOS.Settings.Display.Internal.BrightnessSliderAdjusted",
      /*expected_count=*/0);
}

// Test the behavior when setting the internal display screen brightness (when
// the feature flag is enabled).
TEST_F(DisplaySettingsProviderTest,
       SetInternalDisplayScreenBrightness_FeatureEnabled) {
  feature_list_.Reset();
  feature_list_.InitAndEnableFeature(
      ash::features::kEnableBrightnessControlInSettings);
  provider_->SetBrightnessControlDelegateForTesting(
      brightness_control_delegate_.get());

  // No histograms should have been recorded yet.
  histogram_tester_.ExpectTotalCount(
      "ChromeOS.Settings.Display.Internal.BrightnessSliderAdjusted",
      /*expected_count=*/0);

  double first_brightness_percent = 33.3;
  double second_brightness_percent = 44.4;
  double third_brightness_percent = 55.5;
  // Move the brightness slider rapidly in succession.
  provider_->SetInternalDisplayScreenBrightness(first_brightness_percent);
  FastForwardBy(kMetricsDelayTimerInterval / 4);
  provider_->SetInternalDisplayScreenBrightness(second_brightness_percent);
  FastForwardBy(kMetricsDelayTimerInterval / 4);
  provider_->SetInternalDisplayScreenBrightness(third_brightness_percent);

  // The BrightnessControlDelegate should have been called with the most recent
  // brightness percent.
  EXPECT_EQ(third_brightness_percent,
            brightness_control_delegate_->brightness_percent());
  // The BrightnessChangeSource should indicate that this change came from the
  // Settings app.
  EXPECT_EQ(BrightnessControlDelegate::BrightnessChangeSource::kSettingsApp,
            brightness_control_delegate_->last_brightness_change_source());

  // Wait for the metrics delay timer to resolve.
  FastForwardBy(kMetricsDelayTimerInterval);

  // Histogram should have been recorded for this change, but only for the most
  // recent brightness percent.
  histogram_tester_.ExpectTotalCount(
      "ChromeOS.Settings.Display.Internal.BrightnessSliderAdjusted",
      /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.Display.Internal.BrightnessSliderAdjusted",
      /*sample=*/first_brightness_percent,
      /*expected_count=*/0);
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.Display.Internal.BrightnessSliderAdjusted",
      /*sample=*/second_brightness_percent,
      /*expected_count=*/0);
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.Display.Internal.BrightnessSliderAdjusted",
      /*sample=*/third_brightness_percent,
      /*expected_count=*/1);
}

// Test the behavior when setting the internal display screen brightness (when
// the feature flag is disabled).
TEST_F(DisplaySettingsProviderTest,
       SetAmbientLightSensorEnabled_FeatureDisabled) {
  // No histograms should have been recorded.
  histogram_tester_.ExpectTotalCount(
      "ChromeOS.Settings.Display.Internal.AutoBrightnessEnabled",
      /*expected_count=*/0);

  // Set the ambient_light_sensor_enabled with a sentinel value, so we can test
  // that the value doesn't change if the feature flag is disabled.
  bool initial_sensor_enabled = true;
  brightness_control_delegate_->SetAmbientLightSensorEnabled(
      initial_sensor_enabled,
      BrightnessControlDelegate::AmbientLightSensorEnabledChangeSource::
          kSettingsApp);

  provider_->SetBrightnessControlDelegateForTesting(
      brightness_control_delegate_.get());

  bool expected_sensor_enabled = false;
  provider_->SetInternalDisplayAmbientLightSensorEnabled(
      expected_sensor_enabled);

  // When feature flag is disabled, setting the ambient light sensor value has
  // no effect, and the value should be equal to the initial value.
  EXPECT_EQ(initial_sensor_enabled,
            brightness_control_delegate_->is_ambient_light_sensor_enabled());

  // When the feature flag is disabled, metrics should not be recorded either.
  histogram_tester_.ExpectTotalCount(
      "ChromeOS.Settings.Display.Internal.AutoBrightnessEnabled",
      /*expected_count=*/0);
}

// Test the behavior when setting the internal display screen brightness (when
// the feature flag is enabled).
TEST_F(DisplaySettingsProviderTest,
       SetAmbientLightSensorEnabled_FeatureEnabled) {
  feature_list_.Reset();
  feature_list_.InitAndEnableFeature(
      ash::features::kEnableBrightnessControlInSettings);

  // No histograms should have been recorded yet.
  histogram_tester_.ExpectTotalCount(
      "ChromeOS.Settings.Display.Internal.AutoBrightnessEnabled",
      /*expected_count=*/0);

  // Set the ambient_light_sensor_enabled with a sentinel value, so we can test
  // that the value changes if the feature flag is enabled.
  bool initial_sensor_enabled = true;
  brightness_control_delegate_->SetAmbientLightSensorEnabled(
      initial_sensor_enabled,
      BrightnessControlDelegate::AmbientLightSensorEnabledChangeSource::
          kSettingsApp);

  provider_->SetBrightnessControlDelegateForTesting(
      brightness_control_delegate_.get());

  // When feature flag is enabled, setting the ambient light sensor value from
  // the provider should update the actual ambient light sensor value.
  bool expected_sensor_enabled = false;
  provider_->SetInternalDisplayAmbientLightSensorEnabled(
      expected_sensor_enabled);
  EXPECT_EQ(expected_sensor_enabled,
            brightness_control_delegate_->is_ambient_light_sensor_enabled());

  // Metrics should be recorded for this change.
  histogram_tester_.ExpectTotalCount(
      "ChromeOS.Settings.Display.Internal.AutoBrightnessEnabled",
      /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.Display.Internal.AutoBrightnessEnabled",
      /*sample=*/expected_sensor_enabled,
      /*expected_count=*/1);

  // Re-enabling the sensor from the provider should also work.
  bool expected_sensor_enabled2 = true;
  provider_->SetInternalDisplayAmbientLightSensorEnabled(
      expected_sensor_enabled2);
  EXPECT_EQ(expected_sensor_enabled2,
            brightness_control_delegate_->is_ambient_light_sensor_enabled());

  // Metrics should be recorded for this change.
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Settings.Display.Internal.AutoBrightnessEnabled",
      /*sample=*/expected_sensor_enabled2,
      /*expected_count=*/1);
  histogram_tester_.ExpectTotalCount(
      "ChromeOS.Settings.Display.Internal.AutoBrightnessEnabled",
      /*expected_count=*/2);
}

// Test that the ambient light sensor observer returns the correct information
// when the ambient light sensor status changes.
TEST_F(DisplaySettingsProviderTest, AmbientLightSensorObservation) {
  feature_list_.Reset();
  feature_list_.InitAndEnableFeature(
      ash::features::kEnableBrightnessControlInSettings);

  FakeAmbientLightSensorObserver fake_observer;
  base::test::TestFuture<bool> future;

  provider_->SetInternalDisplayAmbientLightSensorEnabled(false);

  provider_->ObserveAmbientLightSensor(
      fake_observer.receiver.BindNewPipeAndPassRemote(), future.GetCallback());
  base::RunLoop().RunUntilIdle();

  // The TestFuture should have been called with 'false', indicating that the
  // ambient light sensor is not enabled.
  EXPECT_FALSE(future.Get());

  // Observer should not have been called yet.
  EXPECT_EQ(0u, fake_observer.num_ambient_light_sensor_enabled_changed_calls());

  {
    bool is_ambient_light_sensor_enabled = true;
    power_manager::AmbientLightSensorChange change;
    change.set_cause(
        power_manager::AmbientLightSensorChange_Cause_BRIGHTNESS_USER_REQUEST);
    change.set_sensor_enabled(is_ambient_light_sensor_enabled);
    provider_->AmbientLightSensorEnabledChanged(change);

    fake_observer.WaitForAmbientLightSensorEnabledChanged();

    // Observer should have been called.
    EXPECT_EQ(1u,
              fake_observer.num_ambient_light_sensor_enabled_changed_calls());
    EXPECT_EQ(is_ambient_light_sensor_enabled,
              fake_observer.is_ambient_light_sensor_enabled());
  }

  {
    bool is_ambient_light_sensor_enabled = false;
    power_manager::AmbientLightSensorChange change;
    change.set_cause(
        power_manager::AmbientLightSensorChange_Cause_BRIGHTNESS_USER_REQUEST);
    change.set_sensor_enabled(is_ambient_light_sensor_enabled);
    provider_->AmbientLightSensorEnabledChanged(change);

    fake_observer.WaitForAmbientLightSensorEnabledChanged();

    // Observer should have been called a second time.
    EXPECT_EQ(2u,
              fake_observer.num_ambient_light_sensor_enabled_changed_calls());
    EXPECT_EQ(is_ambient_light_sensor_enabled,
              fake_observer.is_ambient_light_sensor_enabled());
  }
}

// Test the behavior when setting the internal display screen brightness (when
// the feature flag is enabled).
TEST_F(DisplaySettingsProviderTest, HasAmbientLightSensor) {
  // Configure the BrightnessControlDelegate to return that the device does have
  // at least one ambient light sensor.
  brightness_control_delegate_->set_has_ambient_light_sensor(true);
  provider_->SetBrightnessControlDelegateForTesting(
      brightness_control_delegate_.get());

  provider_->HasAmbientLightSensor(
      base::BindOnce([](bool has_ambient_light_sensor) {
        EXPECT_TRUE(has_ambient_light_sensor);
      }));

  // Configure the BrightnessControlDelegate to return that the device does not
  // have an ambient light sensor.
  brightness_control_delegate_->set_has_ambient_light_sensor(false);

  provider_->HasAmbientLightSensor(
      base::BindOnce([](bool has_ambient_light_sensor) {
        EXPECT_FALSE(has_ambient_light_sensor);
      }));
}

TEST_F(DisplaySettingsProviderTest, RecordUserInitiatedALSDisabledCause) {
  feature_list_.Reset();
  feature_list_.InitAndEnableFeature(
      ash::features::kEnableBrightnessControlInSettings);

  // No histograms should have been recorded yet.
  histogram_tester_.ExpectTotalCount(
      "ChromeOS.Settings.Display.Internal.UserInitiated."
      "AmbientLightSensorDisabledCause",
      /*expected_count=*/0);

  // Verify histogram recording when ALS is disabled via settings app.
  {
    power_manager::AmbientLightSensorChange cause_settings_app;
    cause_settings_app.set_sensor_enabled(false);
    cause_settings_app.set_cause(
        power_manager::
            AmbientLightSensorChange_Cause_USER_REQUEST_SETTINGS_APP);
    provider_->AmbientLightSensorEnabledChanged(cause_settings_app);
    histogram_tester_.ExpectUniqueSample(
        "ChromeOS.Settings.Display.Internal.UserInitiated."
        "AmbientLightSensorDisabledCause",
        DisplaySettingsProvider::
            UserInitiatedDisplayAmbientLightSensorDisabledCause::
                kUserRequestSettingsApp,
        1);
  }

  // Ensure enabling ALS does not emit histogram.
  {
    power_manager::AmbientLightSensorChange cause_settings_app;
    cause_settings_app.set_sensor_enabled(true);
    cause_settings_app.set_cause(
        power_manager::AmbientLightSensorChange_Cause_BRIGHTNESS_USER_REQUEST);
    provider_->AmbientLightSensorEnabledChanged(cause_settings_app);
    histogram_tester_.ExpectTotalCount(
        "ChromeOS.Settings.Display.Internal.UserInitiated."
        "AmbientLightSensorDisabledCause",
        /*expected_count=*/1);
  }

  // Test histogram update when ALS is disabled due to brightness change.
  {
    power_manager::AmbientLightSensorChange cause_settings_app;
    cause_settings_app.set_sensor_enabled(false);
    cause_settings_app.set_cause(
        power_manager::AmbientLightSensorChange_Cause_BRIGHTNESS_USER_REQUEST);
    provider_->AmbientLightSensorEnabledChanged(cause_settings_app);
    histogram_tester_.ExpectBucketCount(
        "ChromeOS.Settings.Display.Internal.UserInitiated."
        "AmbientLightSensorDisabledCause",
        DisplaySettingsProvider::
            UserInitiatedDisplayAmbientLightSensorDisabledCause::
                kBrightnessUserRequest,
        1);
  }
}

}  // namespace ash::settings
