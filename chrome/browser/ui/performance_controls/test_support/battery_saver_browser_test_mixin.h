// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_TEST_SUPPORT_BATTERY_SAVER_BROWSER_TEST_MIXIN_H_
#define CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_TEST_SUPPORT_BATTERY_SAVER_BROWSER_TEST_MIXIN_H_

#include <concepts>

#include "base/power_monitor/battery_level_provider.h"
#include "base/power_monitor/battery_state_sampler.h"
#include "base/test/power_monitor_test_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/pref_service.h"

// Template to be used as a mixin class for battery saver tests extending
// InteractiveBrowserTest.
template <typename T>
  requires(std::derived_from<T, InProcessBrowserTest>)
class BatterySaverBrowserTestMixin : public T {
 public:
  template <class... Args>
  explicit BatterySaverBrowserTestMixin(Args&&... args)
      : T(std::forward<Args>(args)...) {}

  BatterySaverBrowserTestMixin(const BatterySaverBrowserTestMixin&) = delete;
  BatterySaverBrowserTestMixin& operator=(const BatterySaverBrowserTestMixin&) =
      delete;

  void SetUp() override {
    SetUpFakeBatterySampler();

    T::SetUp();
  }

  // Tests can configure battery state by overriding this method.
  virtual base::BatteryLevelProvider::BatteryState GetFakeBatteryState() {
    return base::test::TestBatteryLevelProvider::CreateBatteryState();
  }

  void SetBatterySaverModeEnabled(bool enabled) {
    auto mode = enabled ? performance_manager::user_tuning::prefs::
                              BatterySaverModeState::kEnabled
                        : performance_manager::user_tuning::prefs::
                              BatterySaverModeState::kDisabled;
    g_browser_process->local_state()->SetInteger(
        performance_manager::user_tuning::prefs::kBatterySaverModeState,
        static_cast<int>(mode));
  }

 private:
  void SetUpFakeBatterySampler() {
    auto test_sampling_event_source =
        std::make_unique<base::test::TestSamplingEventSource>();
    auto test_battery_level_provider =
        std::make_unique<base::test::TestBatteryLevelProvider>();

    test_battery_level_provider->SetBatteryState(
        BatterySaverBrowserTestMixin::GetFakeBatteryState());

    battery_state_sampler_ =
        base::BatteryStateSampler::CreateInstanceForTesting(
            std::move(test_sampling_event_source),
            std::move(test_battery_level_provider));
  }

  std::unique_ptr<base::BatteryStateSampler> battery_state_sampler_;
};

#endif  // CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_TEST_SUPPORT_BATTERY_SAVER_BROWSER_TEST_MIXIN_H_
