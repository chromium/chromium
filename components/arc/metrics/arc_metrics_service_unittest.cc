// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/metrics/arc_metrics_service.h"

#include <algorithm>
#include <array>
#include <map>
#include <utility>
#include <vector>

#include "ash/constants/app_types.h"
#include "base/metrics/histogram_samples.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "components/arc/arc_prefs.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/metrics/arc_metrics_constants.h"
#include "components/arc/metrics/stability_metrics_manager.h"
#include "components/arc/test/test_browser_context.h"
#include "components/prefs/testing_pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"

namespace arc {
namespace {

// The event names the container sends to Chrome.
constexpr std::array<const char*, 11> kBootEvents{
    "boot_progress_start",
    "boot_progress_preload_start",
    "boot_progress_preload_end",
    "boot_progress_system_run",
    "boot_progress_pms_start",
    "boot_progress_pms_system_scan_start",
    "boot_progress_pms_data_scan_start",
    "boot_progress_pms_scan_end",
    "boot_progress_pms_ready",
    "boot_progress_ams_ready",
    "boot_progress_enable_screen"};

constexpr const char kBootProgressArcUpgraded[] = "boot_progress_arc_upgraded";

constexpr char kAppTypeArcAppLauncher[] = "ArcAppLauncher";
constexpr char kAppTypeArcOther[] = "ArcOther";
constexpr char kAppTypeFirstParty[] = "FirstParty";
constexpr char kAppTypeGmsCore[] = "GmsCore";
constexpr char kAppTypePlayStore[] = "PlayStore";
constexpr char kAppTypeSystemServer[] = "SystemServer";
constexpr char kAppTypeSystem[] = "SystemApp";
constexpr char kAppTypeOther[] = "Other";
constexpr char kAppOverall[] = "Overall";

constexpr std::array<const char*, 9> kAppTypes{
    kAppTypeArcAppLauncher, kAppTypeArcOther,  kAppTypeFirstParty,
    kAppTypeGmsCore,        kAppTypePlayStore, kAppTypeSystemServer,
    kAppTypeSystem,         kAppTypeOther,     kAppOverall,
};

std::string CreateAnrKey(const std::string& app_type, mojom::AnrType type) {
  std::stringstream output;
  output << app_type << "/" << type;
  return output.str();
}

mojom::AnrPtr GetAnr(mojom::AnrSource source, mojom::AnrType type) {
  return mojom::Anr::New(type, source);
}

void VerifyAnr(const base::HistogramTester& tester,
               const std::map<std::string, int>& expectation) {
  std::map<std::string, int> current;
  for (const char* app_type : kAppTypes) {
    const std::vector<base::Bucket> buckets =
        tester.GetAllSamples("Arc.Anr." + std::string(app_type));
    for (const auto& bucket : buckets) {
      current[CreateAnrKey(app_type, static_cast<mojom::AnrType>(bucket.min))] =
          bucket.count;
    }
  }
  EXPECT_EQ(expectation, current);
}

class ArcMetricsServiceTest : public testing::Test {
 public:
  ArcMetricsServiceTest(const ArcMetricsServiceTest&) = delete;
  ArcMetricsServiceTest& operator=(const ArcMetricsServiceTest&) = delete;

 protected:
  ArcMetricsServiceTest() {
    prefs::RegisterLocalStatePrefs(local_state_.registry());
    StabilityMetricsManager::Initialize(&local_state_);
    chromeos::PowerManagerClient::InitializeFake();
    chromeos::SessionManagerClient::InitializeFakeInMemory();
    chromeos::FakeSessionManagerClient::Get()->set_arc_available(true);

    arc_service_manager_ = std::make_unique<ArcServiceManager>();
    context_ = std::make_unique<TestBrowserContext>();
    prefs::RegisterProfilePrefs(context_->pref_registry());
    service_ =
        ArcMetricsService::GetForBrowserContextForTesting(context_.get());

    CreateFakeWindows();
  }

  ~ArcMetricsServiceTest() override {
    fake_non_arc_window_.reset();
    fake_arc_window_.reset();

    context_.reset();
    arc_service_manager_.reset();

    chromeos::SessionManagerClient::Shutdown();
    chromeos::PowerManagerClient::Shutdown();
    StabilityMetricsManager::Shutdown();
  }

  ArcMetricsService* service() { return service_; }

  void SetArcStartTimeInMs(uint64_t arc_start_time_in_ms) {
    const base::TimeTicks arc_start_time =
        base::Milliseconds(arc_start_time_in_ms) + base::TimeTicks();
    chromeos::FakeSessionManagerClient::Get()->set_arc_start_time(
        arc_start_time);
  }

  std::vector<mojom::BootProgressEventPtr> GetBootProgressEvents(
      uint64_t start_in_ms,
      uint64_t step_in_ms) {
    std::vector<mojom::BootProgressEventPtr> events;
    for (size_t i = 0; i < kBootEvents.size(); ++i) {
      events.emplace_back(mojom::BootProgressEvent::New(
          kBootEvents[i], start_in_ms + (step_in_ms * i)));
    }
    return events;
  }

  aura::Window* fake_arc_window() { return fake_arc_window_.get(); }
  aura::Window* fake_non_arc_window() { return fake_non_arc_window_.get(); }

 private:
  void CreateFakeWindows() {
    fake_arc_window_.reset(aura::test::CreateTestWindowWithId(
        /*id=*/0, nullptr));
    fake_arc_window_->SetProperty(aura::client::kAppType,
                                  static_cast<int>(ash::AppType::ARC_APP));
    fake_non_arc_window_.reset(aura::test::CreateTestWindowWithId(
        /*id=*/1, nullptr));
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingPrefServiceSimple local_state_;
  session_manager::SessionManager session_manager_;

  std::unique_ptr<ArcServiceManager> arc_service_manager_;
  std::unique_ptr<TestBrowserContext> context_;
  ArcMetricsService* service_;

  std::unique_ptr<aura::Window> fake_arc_window_;
  std::unique_ptr<aura::Window> fake_non_arc_window_;
};

// Tests that ReportBootProgress() actually records UMA stats.
TEST_F(ArcMetricsServiceTest, ReportBootProgress_FirstBoot) {
  // Start the full ARC container at t=10. Also set boot_progress_start to 10,
  // boot_progress_preload_start to 11, and so on.
  constexpr uint64_t kArcStartTimeMs = 10;
  SetArcStartTimeInMs(kArcStartTimeMs);
  std::vector<mojom::BootProgressEventPtr> events(
      GetBootProgressEvents(kArcStartTimeMs, 1 /* step_in_ms */));

  // Call ReportBootProgress() and then confirm that
  // Arc.boot_progress_start.FirstBoot is recorded with 0 (ms),
  // Arc.boot_progress_preload_start.FirstBoot is with 1 (ms), etc.
  base::HistogramTester tester;
  service()->ReportBootProgress(std::move(events), mojom::BootType::FIRST_BOOT);
  base::RunLoop().RunUntilIdle();
  for (size_t i = 0; i < kBootEvents.size(); ++i) {
    tester.ExpectUniqueSample(
        std::string("Arc.") + kBootEvents[i] + ".FirstBoot", i,
        1 /* count of the sample */);
  }
  // Confirm that Arc.AndroidBootTime.FirstBoot is also recorded, and has the
  // same value as "Arc.boot_progress_enable_screen.FirstBoot".
  std::unique_ptr<base::HistogramSamples> samples =
      tester.GetHistogramSamplesSinceCreation(
          "Arc." + std::string(kBootEvents.back()) + ".FirstBoot");
  ASSERT_TRUE(samples.get());
  tester.ExpectUniqueSample("Arc.AndroidBootTime.FirstBoot", samples->sum(), 1);
}

// Does the same but with negative values and FIRST_BOOT_AFTER_UPDATE.
TEST_F(ArcMetricsServiceTest, ReportBootProgress_FirstBootAfterUpdate) {
  // Start the full ARC container at t=10. Also set boot_progress_start to 5,
  // boot_progress_preload_start to 7, and so on. This can actually happen
  // because the mini container can finish up to boot_progress_preload_end
  // before the full container is started.
  constexpr uint64_t kArcStartTimeMs = 10;
  SetArcStartTimeInMs(kArcStartTimeMs);
  std::vector<mojom::BootProgressEventPtr> events(
      GetBootProgressEvents(kArcStartTimeMs - 5, 2 /* step_in_ms */));

  // Call ReportBootProgress() and then confirm that
  // Arc.boot_progress_start.FirstBoot is recorded with 0 (ms),
  // Arc.boot_progress_preload_start.FirstBoot is with 0 (ms), etc. Unlike our
  // performance dashboard where negative performance numbers are treated as-is,
  // UMA treats them as zeros.
  base::HistogramTester tester;
  // This time, use mojom::BootType::FIRST_BOOT_AFTER_UPDATE.
  service()->ReportBootProgress(std::move(events),
                                mojom::BootType::FIRST_BOOT_AFTER_UPDATE);
  base::RunLoop().RunUntilIdle();
  for (size_t i = 0; i < kBootEvents.size(); ++i) {
    const int expected = std::max<int>(0, i * 2 - 5);
    tester.ExpectUniqueSample(
        std::string("Arc.") + kBootEvents[i] + ".FirstBootAfterUpdate",
        expected, 1);
  }
  std::unique_ptr<base::HistogramSamples> samples =
      tester.GetHistogramSamplesSinceCreation(
          "Arc." + std::string(kBootEvents.back()) + ".FirstBootAfterUpdate");
  ASSERT_TRUE(samples.get());
  tester.ExpectUniqueSample("Arc.AndroidBootTime.FirstBootAfterUpdate",
                            samples->sum(), 1);
}

// Does the same but with REGULAR_BOOT.
TEST_F(ArcMetricsServiceTest, ReportBootProgress_RegularBoot) {
  constexpr uint64_t kArcStartTimeMs = 10;
  SetArcStartTimeInMs(kArcStartTimeMs);
  std::vector<mojom::BootProgressEventPtr> events(
      GetBootProgressEvents(kArcStartTimeMs - 5, 2 /* step_in_ms */));

  base::HistogramTester tester;
  service()->ReportBootProgress(std::move(events),
                                mojom::BootType::REGULAR_BOOT);
  base::RunLoop().RunUntilIdle();
  for (size_t i = 0; i < kBootEvents.size(); ++i) {
    const int expected = std::max<int>(0, i * 2 - 5);
    tester.ExpectUniqueSample(
        std::string("Arc.") + kBootEvents[i] + ".RegularBoot", expected, 1);
  }
  std::unique_ptr<base::HistogramSamples> samples =
      tester.GetHistogramSamplesSinceCreation(
          "Arc." + std::string(kBootEvents.back()) + ".RegularBoot");
  ASSERT_TRUE(samples.get());
  tester.ExpectUniqueSample("Arc.AndroidBootTime.RegularBoot", samples->sum(),
                            1);
}

// Tests that no UMA is recorded when nothing is reported.
TEST_F(ArcMetricsServiceTest, ReportBootProgress_EmptyResults) {
  SetArcStartTimeInMs(100);
  std::vector<mojom::BootProgressEventPtr> events;  // empty

  base::HistogramTester tester;
  service()->ReportBootProgress(std::move(events), mojom::BootType::FIRST_BOOT);
  base::RunLoop().RunUntilIdle();
  for (size_t i = 0; i < kBootEvents.size(); ++i) {
    tester.ExpectTotalCount(std::string("Arc.") + kBootEvents[i] + ".FirstBoot",
                            0);
  }
  tester.ExpectTotalCount("Arc.AndroidBootTime.FirstBoot", 0);
}

// Tests that no UMA is recorded when BootType is invalid.
TEST_F(ArcMetricsServiceTest, ReportBootProgress_InvalidBootType) {
  SetArcStartTimeInMs(100);
  std::vector<mojom::BootProgressEventPtr> events(
      GetBootProgressEvents(123, 456));
  base::HistogramTester tester;
  service()->ReportBootProgress(std::move(events), mojom::BootType::UNKNOWN);
  base::RunLoop().RunUntilIdle();
  for (const std::string& suffix :
       {".FirstBoot", ".FirstBootAfterUpdate", ".RegularBoot"}) {
    tester.ExpectTotalCount("Arc." + (kBootEvents.front() + suffix), 0);
    tester.ExpectTotalCount("Arc." + (kBootEvents.back() + suffix), 0);
    tester.ExpectTotalCount("Arc.AndroidBootTime" + suffix, 0);
  }
}

TEST_F(ArcMetricsServiceTest, ReportNativeBridge) {
  // SetArcNativeBridgeType should be called once ArcMetricsService is
  // constructed.
  EXPECT_EQ(StabilityMetricsManager::Get()->GetArcNativeBridgeType(),
            NativeBridgeType::UNKNOWN);
  service()->ReportNativeBridge(mojom::NativeBridgeType::NONE);
  EXPECT_EQ(StabilityMetricsManager::Get()->GetArcNativeBridgeType(),
            NativeBridgeType::NONE);
  service()->ReportNativeBridge(mojom::NativeBridgeType::HOUDINI);
  EXPECT_EQ(StabilityMetricsManager::Get()->GetArcNativeBridgeType(),
            NativeBridgeType::HOUDINI);
  service()->ReportNativeBridge(mojom::NativeBridgeType::NDK_TRANSLATION);
  EXPECT_EQ(StabilityMetricsManager::Get()->GetArcNativeBridgeType(),
            NativeBridgeType::NDK_TRANSLATION);
}

TEST_F(ArcMetricsServiceTest, RecordArcWindowFocusAction) {
  base::HistogramTester tester;

  service()->OnWindowActivated(
      wm::ActivationChangeObserver::ActivationReason::INPUT_EVENT,
      fake_arc_window(), nullptr);

  tester.ExpectBucketCount(
      "Arc.UserInteraction",
      static_cast<int>(UserInteractionType::APP_CONTENT_WINDOW_INTERACTION), 1);
}

TEST_F(ArcMetricsServiceTest, RecordNothingNonArcWindowFocusAction) {
  base::HistogramTester tester;

  // Focus an ARC window once so that the histogram is created.
  service()->OnWindowActivated(
      wm::ActivationChangeObserver::ActivationReason::INPUT_EVENT,
      fake_arc_window(), nullptr);
  tester.ExpectBucketCount(
      "Arc.UserInteraction",
      static_cast<int>(UserInteractionType::APP_CONTENT_WINDOW_INTERACTION), 1);

  // Focusing a non-ARC window should not increase the bucket count.
  service()->OnWindowActivated(
      wm::ActivationChangeObserver::ActivationReason::INPUT_EVENT,
      fake_non_arc_window(), nullptr);

  tester.ExpectBucketCount(
      "Arc.UserInteraction",
      static_cast<int>(UserInteractionType::APP_CONTENT_WINDOW_INTERACTION), 1);
}

TEST_F(ArcMetricsServiceTest, GetArcStartTimeFromEvents) {
  constexpr uint64_t kArcStartTimeMs = 10;
  std::vector<mojom::BootProgressEventPtr> events(
      GetBootProgressEvents(kArcStartTimeMs, 1 /* step_in_ms */));
  events.emplace_back(
      mojom::BootProgressEvent::New(kBootProgressArcUpgraded, kArcStartTimeMs));

  absl::optional<base::TimeTicks> arc_start_time =
      service()->GetArcStartTimeFromEvents(events);
  EXPECT_TRUE(arc_start_time.has_value());
  EXPECT_EQ(*arc_start_time, base::Milliseconds(10) + base::TimeTicks());

  // Check that the upgrade event was removed from events.
  EXPECT_TRUE(std::none_of(
      events.begin(), events.end(), [](const mojom::BootProgressEventPtr& ev) {
        return ev->event.compare(kBootProgressArcUpgraded) == 0;
      }));
}

TEST_F(ArcMetricsServiceTest, GetArcStartTimeFromEvents_NoArcUpgradedEvent) {
  constexpr uint64_t kArcStartTimeMs = 10;
  std::vector<mojom::BootProgressEventPtr> events(
      GetBootProgressEvents(kArcStartTimeMs, 1 /* step_in_ms */));

  absl::optional<base::TimeTicks> arc_start_time =
      service()->GetArcStartTimeFromEvents(events);
  EXPECT_FALSE(arc_start_time.has_value());
}

TEST_F(ArcMetricsServiceTest, UserInteractionObserver) {
  class Observer : public ArcMetricsService::UserInteractionObserver {
   public:
    void OnUserInteraction(UserInteractionType type) override {
      this->type = type;
    }
    absl::optional<UserInteractionType> type;
  } observer;

  service()->AddUserInteractionObserver(&observer);

  // This calls RecordArcUserInteraction() with APP_CONTENT_WINDOW_INTERACTION.
  service()->OnWindowActivated(
      wm::ActivationChangeObserver::ActivationReason::INPUT_EVENT,
      fake_arc_window(), nullptr);
  ASSERT_TRUE(observer.type);
  EXPECT_EQ(UserInteractionType::APP_CONTENT_WINDOW_INTERACTION,
            *observer.type);

  service()->RemoveUserInteractionObserver(&observer);
}

TEST_F(ArcMetricsServiceTest, ArcAnr) {
  base::HistogramTester tester;
  std::map<std::string, int> expectation;

  service()->ReportAnr(
      GetAnr(mojom::AnrSource::OTHER, mojom::AnrType::UNKNOWN));
  expectation[CreateAnrKey(kAppOverall, mojom::AnrType::UNKNOWN)] = 1;
  expectation[CreateAnrKey(kAppTypeOther, mojom::AnrType::UNKNOWN)] = 1;
  VerifyAnr(tester, expectation);

  service()->ReportAnr(
      GetAnr(mojom::AnrSource::SYSTEM_SERVER, mojom::AnrType::INPUT));
  expectation[CreateAnrKey(kAppOverall, mojom::AnrType::INPUT)] = 1;
  expectation[CreateAnrKey(kAppTypeSystemServer, mojom::AnrType::INPUT)] = 1;
  VerifyAnr(tester, expectation);

  service()->ReportAnr(
      GetAnr(mojom::AnrSource::SYSTEM_SERVER, mojom::AnrType::SERVICE));
  expectation[CreateAnrKey(kAppOverall, mojom::AnrType::SERVICE)] = 1;
  expectation[CreateAnrKey(kAppTypeSystemServer, mojom::AnrType::SERVICE)] = 1;
  VerifyAnr(tester, expectation);

  service()->ReportAnr(
      GetAnr(mojom::AnrSource::GMS_CORE, mojom::AnrType::BROADCAST));
  expectation[CreateAnrKey(kAppOverall, mojom::AnrType::BROADCAST)] = 1;
  expectation[CreateAnrKey(kAppTypeGmsCore, mojom::AnrType::BROADCAST)] = 1;
  VerifyAnr(tester, expectation);

  service()->ReportAnr(
      GetAnr(mojom::AnrSource::PLAY_STORE, mojom::AnrType::CONTENT_PROVIDER));
  expectation[CreateAnrKey(kAppOverall, mojom::AnrType::CONTENT_PROVIDER)] = 1;
  expectation[CreateAnrKey(kAppTypePlayStore,
                           mojom::AnrType::CONTENT_PROVIDER)] = 1;
  VerifyAnr(tester, expectation);

  service()->ReportAnr(
      GetAnr(mojom::AnrSource::FIRST_PARTY, mojom::AnrType::APP_REQUESTED));
  expectation[CreateAnrKey(kAppOverall, mojom::AnrType::APP_REQUESTED)] = 1;
  expectation[CreateAnrKey(kAppTypeFirstParty, mojom::AnrType::APP_REQUESTED)] =
      1;
  VerifyAnr(tester, expectation);

  service()->ReportAnr(
      GetAnr(mojom::AnrSource::ARC_OTHER, mojom::AnrType::INPUT));
  expectation[CreateAnrKey(kAppOverall, mojom::AnrType::INPUT)] = 2;
  expectation[CreateAnrKey(kAppTypeArcOther, mojom::AnrType::INPUT)] = 1;
  VerifyAnr(tester, expectation);

  service()->ReportAnr(
      GetAnr(mojom::AnrSource::ARC_APP_LAUNCHER, mojom::AnrType::SERVICE));
  expectation[CreateAnrKey(kAppOverall, mojom::AnrType::SERVICE)] = 2;
  expectation[CreateAnrKey(kAppTypeArcAppLauncher, mojom::AnrType::SERVICE)] =
      1;
  VerifyAnr(tester, expectation);
}

}  // namespace
}  // namespace arc
