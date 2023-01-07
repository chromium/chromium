// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/guest_os/guest_os_engagement_metrics.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/test/simple_test_tick_clock.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/idle.pb.h"
#include "components/guest_os/guest_os_prefs.h"
#include "components/prefs/testing_pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/test/test_windows.h"

namespace guest_os {
namespace {

constexpr char kUmaName[] = "Foo";
constexpr char kPrefPrefix[] = "Bar";

constexpr char kHistogramTotal[] = "Total";
constexpr char kHistogramForeground[] = "Foreground";
constexpr char kHistogramBackground[] = "Background";
constexpr char kHistogramActiveTotal[] = "FooTotal";

class GuestOsEngagementMetricsTest : public testing::Test {
 protected:
  GuestOsEngagementMetricsTest() = default;

  GuestOsEngagementMetricsTest(const GuestOsEngagementMetricsTest&) = delete;
  GuestOsEngagementMetricsTest& operator=(const GuestOsEngagementMetricsTest&) =
      delete;

  void SetUp() override {
    chromeos::PowerManagerClient::InitializeFake();
    ash::SessionManagerClient::InitializeFakeInMemory();
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();

    matched_window_.reset(aura::test::CreateTestWindowWithId(0, nullptr));
    non_matched_window_.reset(aura::test::CreateTestWindowWithId(0, nullptr));

    prefs::RegisterEngagementProfilePrefs(pref_service_->registry(),
                                          kPrefPrefix);

    // The code doesn't work for correctly for a clock just at the epoch so
    // advance by a day first.
    test_clock_.Advance(base::Days(1));
    CreateEngagementMetrics();
    SetSessionState(session_manager::SessionState::ACTIVE);
  }

  void TearDown() override {
    engagement_metrics_.reset();
    non_matched_window_.reset();
    matched_window_.reset();
    pref_service_.reset();
    ash::SessionManagerClient::Shutdown();
    chromeos::PowerManagerClient::Shutdown();
  }

  GuestOsEngagementMetrics* engagement_metrics() {
    return engagement_metrics_.get();
  }

  void SetSessionState(session_manager::SessionState state) {
    session_manager_.SetSessionState(state);
  }

  void SetScreenDimmed(bool is_screen_dimmed) {
    power_manager::ScreenIdleState screen_idle_state;
    screen_idle_state.set_dimmed(is_screen_dimmed);
    static_cast<chromeos::FakePowerManagerClient*>(
        chromeos::PowerManagerClient::Get())
        ->SendScreenIdleStateChanged(screen_idle_state);
  }

  void AdvanceSeconds(int seconds) {
    test_tick_clock_.Advance(base::Seconds(seconds));
  }

  void FocusMatchedWindow() {
    engagement_metrics()->OnWindowActivated(
        wm::ActivationChangeObserver::ActivationReason::INPUT_EVENT,
        matched_window_.get(), nullptr);
  }

  void FocusNonMatchedWindow() {
    engagement_metrics()->OnWindowActivated(
        wm::ActivationChangeObserver::ActivationReason::INPUT_EVENT,
        non_matched_window_.get(), nullptr);
  }

  void TriggerRecordEngagementTimeToUma() {
    // Trigger UMA record by changing to next day.
    test_clock_.Advance(base::Days(1));
    engagement_metrics_->OnSessionStateChanged();
  }

  void ExpectTime(const std::string& histogram, int seconds) {
    tester_.ExpectTimeBucketCount("Foo.EngagementTime." + histogram,
                                  base::Seconds(seconds), 1);
  }

  void DestroyEngagementMetrics() { engagement_metrics_.reset(); }

  void CreateEngagementMetrics() {
    engagement_metrics_ =
        GuestOsEngagementMetrics::GetEngagementMetricsForTesting(
            pref_service_.get(),
            base::BindRepeating(&GuestOsEngagementMetricsTest::MatchWindow,
                                base::Unretained(this)),
            kPrefPrefix, kUmaName, &test_clock_, &test_tick_clock_);
  }

 private:
  bool MatchWindow(const aura::Window* window) {
    return window == matched_window_.get();
  }

  content::BrowserTaskEnvironment task_environment_;
  session_manager::SessionManager session_manager_;

  base::SimpleTestTickClock test_tick_clock_;
  base::SimpleTestClock test_clock_;

  base::HistogramTester tester_;

  std::unique_ptr<TestingPrefServiceSimple> pref_service_;

  std::unique_ptr<aura::Window> matched_window_;
  std::unique_ptr<aura::Window> non_matched_window_;

  std::unique_ptr<GuestOsEngagementMetrics> engagement_metrics_;
};

TEST_F(GuestOsEngagementMetricsTest, RecordEngagementTimeSessionLocked) {
  SetSessionState(session_manager::SessionState::LOCKED);
  AdvanceSeconds(1);
  SetSessionState(session_manager::SessionState::ACTIVE);
  AdvanceSeconds(5);
  SetSessionState(session_manager::SessionState::LOCKED);
  AdvanceSeconds(10);

  TriggerRecordEngagementTimeToUma();
  ExpectTime(kHistogramTotal, 5);
  ExpectTime(kHistogramForeground, 0);
  ExpectTime(kHistogramBackground, 0);
  ExpectTime(kHistogramActiveTotal, 0);
}

TEST_F(GuestOsEngagementMetricsTest, RecordEngagementTimeScreenDimmed) {
  SetScreenDimmed(true);
  AdvanceSeconds(1);
  SetScreenDimmed(false);
  AdvanceSeconds(5);
  SetScreenDimmed(true);
  AdvanceSeconds(10);

  TriggerRecordEngagementTimeToUma();
  ExpectTime(kHistogramTotal, 5);
  ExpectTime(kHistogramForeground, 0);
  ExpectTime(kHistogramBackground, 0);
  ExpectTime(kHistogramActiveTotal, 0);
}

TEST_F(GuestOsEngagementMetricsTest, RecordEngagementTimeChangeFocus) {
  FocusMatchedWindow();
  AdvanceSeconds(2);
  FocusNonMatchedWindow();
  AdvanceSeconds(4);
  FocusMatchedWindow();
  AdvanceSeconds(10);
  FocusNonMatchedWindow();
  AdvanceSeconds(20);

  // No background time is recorded as background activity is based on calls to
  // SetBackgroundActive and not background windows.
  TriggerRecordEngagementTimeToUma();
  ExpectTime(kHistogramTotal, 36);
  ExpectTime(kHistogramForeground, 12);
  ExpectTime(kHistogramBackground, 0);
  ExpectTime(kHistogramActiveTotal, 12);
}

TEST_F(GuestOsEngagementMetricsTest, RecordEngagementTimeBackgroundActive) {
  AdvanceSeconds(10);
  engagement_metrics()->SetBackgroundActive(true);
  AdvanceSeconds(5);
  engagement_metrics()->SetBackgroundActive(false);
  AdvanceSeconds(1);

  TriggerRecordEngagementTimeToUma();
  ExpectTime(kHistogramTotal, 16);
  ExpectTime(kHistogramForeground, 0);
  ExpectTime(kHistogramBackground, 5);
  ExpectTime(kHistogramActiveTotal, 5);
}

TEST_F(GuestOsEngagementMetricsTest,
       RecordEngagementTimeBackgroundAndForeground) {
  AdvanceSeconds(1);
  engagement_metrics()->SetBackgroundActive(true);
  AdvanceSeconds(2);
  FocusMatchedWindow();
  AdvanceSeconds(4);
  FocusNonMatchedWindow();
  AdvanceSeconds(10);
  engagement_metrics()->SetBackgroundActive(false);
  AdvanceSeconds(20);

  TriggerRecordEngagementTimeToUma();
  ExpectTime(kHistogramTotal, 37);
  ExpectTime(kHistogramForeground, 4);
  ExpectTime(kHistogramBackground, 12);
  ExpectTime(kHistogramActiveTotal, 16);
}

TEST_F(GuestOsEngagementMetricsTest, RecordEngagementTimeIfDestroyed) {
  AdvanceSeconds(1);  // Total
  engagement_metrics()->SetBackgroundActive(true);
  FocusMatchedWindow();
  AdvanceSeconds(1);  // Total + foreground + active

  DestroyEngagementMetrics();

  AdvanceSeconds(1);  // Nothing

  CreateEngagementMetrics();
  engagement_metrics()->SetBackgroundActive(true);
  FocusNonMatchedWindow();
  AdvanceSeconds(1);  // Total + background + active

  TriggerRecordEngagementTimeToUma();
  ExpectTime(kHistogramTotal, 3);
  ExpectTime(kHistogramForeground, 1);
  ExpectTime(kHistogramBackground, 1);
  ExpectTime(kHistogramActiveTotal, 2);
}

}  // namespace
}  // namespace guest_os
