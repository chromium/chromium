// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/test/app_list_test_api.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/time/time.h"
#include "chrome/browser/ui/ash/assistant/assistant_test_mixin.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/ash/components/assistant/test_support/expect_utils.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/ash/services/assistant/public/cpp/features.h"
#include "chromeos/ash/services/assistant/public/cpp/switches.h"
#include "chromeos/ash/services/assistant/service.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"
#include "content/public/test/browser_test.h"
#include "sandbox/policy/switches.h"

namespace ash::assistant {

namespace {

using test::ExpectResult;

// Please remember to set auth token when running in |kProxy| mode.
constexpr auto kMode = FakeS3Mode::kReplay;
// Update this when you introduce breaking changes to existing tests.
constexpr int kVersion = 1;

constexpr int kStartBrightnessPercent = 50;

inline constexpr char kDlcInstallResultHistogram[] =
    "Assistant.Libassistant.DlcInstallResult";

inline constexpr char kDlcLoadStatusHistogram[] =
    "Assistant.Libassistant.DlcLoadStatus";

// Ensures that |value_| is within the range {min_, max_}. If it isn't, this
// will print a nice error message.
#define EXPECT_WITHIN_RANGE(min_, value_, max_)                \
  ({                                                           \
    EXPECT_TRUE(min_ <= value_ && value_ <= max_)              \
        << "Expected " << value_ << " to be within the range " \
        << "{" << min_ << ", " << max_ << "}.";                \
  })

}  // namespace

// All tests are disabled because LibAssistant V2 binary does not run on Linux
// bot. To run the tests on gLinux, please add
// `--gtest_also_run_disabled_tests`.
class DISABLED_AssistantBrowserTest : public MixinBasedInProcessBrowserTest,
                                      public testing::WithParamInterface<bool> {
 public:
  DISABLED_AssistantBrowserTest()
      : DISABLED_AssistantBrowserTest(/*disable_sandbox=*/true) {}

  explicit DISABLED_AssistantBrowserTest(bool disable_sandbox) {
    // Do not log to file in test. Otherwise multiple tests may create/delete
    // the log file at the same time. See http://crbug.com/1307868.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kDisableLibAssistantLogfile);

    if (disable_sandbox) {
      // In browser tests, the fake_s3_server uses gRPC framework, which is not
      // allowed in the sandbox by default. Instead of enabling and setting up
      // the gRPC policy, we do not enable sandbox in the tests.
      base::CommandLine::ForCurrentProcess()->AppendSwitch(
          sandbox::policy::switches::kNoSandbox);
    }
  }

  DISABLED_AssistantBrowserTest(const DISABLED_AssistantBrowserTest&) = delete;
  DISABLED_AssistantBrowserTest& operator=(
      const DISABLED_AssistantBrowserTest&) = delete;

  ~DISABLED_AssistantBrowserTest() override = default;

  AssistantTestMixin* tester() { return &tester_; }

  void ShowAssistantUi() {
    if (!tester()->IsVisible())
      tester()->PressAssistantKey();

    // Make sure that the app list bubble finished showing.
    AppListTestApi().WaitForBubbleWindow(
        /*wait_for_opening_animation=*/false);
  }

  void CloseAssistantUi() {
    if (tester()->IsVisible())
      tester()->PressAssistantKey();
  }

  void InitializeBrightness() {
    auto* power_manager = chromeos::PowerManagerClient::Get();
    power_manager::SetBacklightBrightnessRequest request;
    request.set_percent(kStartBrightnessPercent);
    request.set_transition(
        power_manager::SetBacklightBrightnessRequest_Transition_INSTANT);
    request.set_cause(
        power_manager::SetBacklightBrightnessRequest_Cause_USER_REQUEST);
    chromeos::PowerManagerClient::Get()->SetScreenBrightness(request);

    // Wait for the initial value to settle.
    ExpectResult(
        true, base::BindLambdaForTesting([&]() {
          constexpr double kEpsilon = 0.1;
          auto current_brightness = tester()->SyncCall(base::BindOnce(
              &chromeos::PowerManagerClient::GetScreenBrightnessPercent,
              base::Unretained(power_manager)));
          return current_brightness &&
                 std::abs(kStartBrightnessPercent -
                          current_brightness.value()) < kEpsilon;
        }));
  }

  void ExpectBrightnessUp() {
    auto* power_manager = chromeos::PowerManagerClient::Get();
    // Check the brightness changes
    ExpectResult(
        true, base::BindLambdaForTesting([&]() {
          constexpr double kEpsilon = 1;
          auto current_brightness = tester()->SyncCall(base::BindOnce(
              &chromeos::PowerManagerClient::GetScreenBrightnessPercent,
              base::Unretained(power_manager)));

          return current_brightness && (current_brightness.value() -
                                        kStartBrightnessPercent) > kEpsilon;
        }));
  }

  void ExpectBrightnessDown() {
    auto* power_manager = chromeos::PowerManagerClient::Get();
    // Check the brightness changes
    ExpectResult(
        true, base::BindLambdaForTesting([&]() {
          constexpr double kEpsilon = 1;
          auto current_brightness = tester()->SyncCall(base::BindOnce(
              &chromeos::PowerManagerClient::GetScreenBrightnessPercent,
              base::Unretained(power_manager)));

          return current_brightness && (kStartBrightnessPercent -
                                        current_brightness.value()) > kEpsilon;
        }));
  }

  base::HistogramTester* histogram_tester() { return &histogram_tester_; }

 private:
  base::HistogramTester histogram_tester_;
  AssistantTestMixin tester_{&mixin_host_, this, embedded_test_server(), kMode,
                             kVersion};
};

class DISABLED_AssistantBrowserTestWithSandbox
    : public DISABLED_AssistantBrowserTest {
 public:
  DISABLED_AssistantBrowserTestWithSandbox()
      : DISABLED_AssistantBrowserTest(/*disable_sandbox=*/false) {}
};

// Tests that Assistant can start up with sandbox.
IN_PROC_BROWSER_TEST_F(DISABLED_AssistantBrowserTestWithSandbox, Ready) {
  tester()->StartAssistantAndWaitForReady();
}

IN_PROC_BROWSER_TEST_F(DISABLED_AssistantBrowserTest,
                       ShouldOpenAssistantUiWhenPressingAssistantKey) {
  tester()->StartAssistantAndWaitForReady();

  tester()->PressAssistantKey();

  // Make sure that the app list bubble finished showing (the app list view gets
  // created asynchronously).
  AppListTestApi().WaitForBubbleWindow(
      /*wait_for_opening_animation=*/false);

  EXPECT_TRUE(tester()->IsVisible());
  histogram_tester()->ExpectTotalCount(kDlcInstallResultHistogram, 1);
  histogram_tester()->ExpectTotalCount(kDlcLoadStatusHistogram, 1);
}

IN_PROC_BROWSER_TEST_F(DISABLED_AssistantBrowserTest,
                       ShouldDisplayTextResponse) {
  tester()->StartAssistantAndWaitForReady();

  ShowAssistantUi();

  tester()->SendTextQuery("test");
  tester()->ExpectAnyOfTheseTextResponses({
      "No one told me there would be a test",
      "You're coming in loud and clear",
      "debug OK",
      "I can assure you, this thing's on",
      "Is this thing on?",
  });
}

IN_PROC_BROWSER_TEST_F(DISABLED_AssistantBrowserTest,
                       ShouldDisplayTextResponseWithTwoContiniousQueries) {
  tester()->StartAssistantAndWaitForReady();

  ShowAssistantUi();

  tester()->SendTextQuery("phone");
  tester()->SendTextQuery("test");
  tester()->ExpectAnyOfTheseTextResponses({
      "No one told me there would be a test",
      "You're coming in loud and clear",
      "debug OK",
      "I can assure you, this thing's on",
      "Is this thing on?",
  });
}

IN_PROC_BROWSER_TEST_F(DISABLED_AssistantBrowserTest,
                       ShouldDisplayCardResponse) {
  tester()->StartAssistantAndWaitForReady();

  ShowAssistantUi();

  ASSERT_TRUE(tester()->IsVisible());

  tester()->SendTextQuery("What is the highest mountain in the world?");
  tester()->ExpectCardResponse("Mount Everest");
}

IN_PROC_BROWSER_TEST_F(DISABLED_AssistantBrowserTest, ShouldTurnUpVolume) {
  tester()->StartAssistantAndWaitForReady();

  ShowAssistantUi();

  ASSERT_TRUE(tester()->IsVisible());

  auto* cras = CrasAudioHandler::Get();
  constexpr int kStartVolumePercent = 50;
  cras->SetOutputVolumePercent(kStartVolumePercent);
  EXPECT_EQ(kStartVolumePercent, cras->GetOutputVolumePercent());

  tester()->SendTextQuery("turn up volume");

  ExpectResult(true, base::BindRepeating(
                         [](CrasAudioHandler* cras) {
                           return cras->GetOutputVolumePercent() >
                                  kStartVolumePercent;
                         },
                         cras));
}

IN_PROC_BROWSER_TEST_F(DISABLED_AssistantBrowserTest, ShouldTurnDownVolume) {
  tester()->StartAssistantAndWaitForReady();

  ShowAssistantUi();

  ASSERT_TRUE(tester()->IsVisible());

  auto* cras = CrasAudioHandler::Get();
  constexpr int kStartVolumePercent = 50;
  cras->SetOutputVolumePercent(kStartVolumePercent);
  EXPECT_EQ(kStartVolumePercent, cras->GetOutputVolumePercent());

  tester()->SendTextQuery("turn down volume");

  ExpectResult(true, base::BindRepeating(
                         [](CrasAudioHandler* cras) {
                           return cras->GetOutputVolumePercent() <
                                  kStartVolumePercent;
                         },
                         cras));
}

IN_PROC_BROWSER_TEST_F(DISABLED_AssistantBrowserTest, ShouldTurnUpBrightness) {
  tester()->StartAssistantAndWaitForReady();

  ShowAssistantUi();

  ASSERT_TRUE(tester()->IsVisible());

  InitializeBrightness();

  tester()->SendTextQuery("turn up brightness");

  ExpectBrightnessUp();
}

IN_PROC_BROWSER_TEST_F(DISABLED_AssistantBrowserTest,
                       ShouldTurnDownBrightness) {
  tester()->StartAssistantAndWaitForReady();

  ShowAssistantUi();

  ASSERT_TRUE(tester()->IsVisible());

  InitializeBrightness();

  tester()->SendTextQuery("turn down brightness");

  ExpectBrightnessDown();
}

IN_PROC_BROWSER_TEST_F(DISABLED_AssistantBrowserTest,
                       ShouldPuntWhenChangingUnsupportedSetting) {
  tester()->StartAssistantAndWaitForReady();

  ShowAssistantUi();

  ASSERT_TRUE(tester()->IsVisible());

  tester()->SendTextQuery("enable night mode");

  tester()->ExpectTextResponse("Night Mode isn't available on your device");
}

// TODO(crbug.com/40142964): Disabled because it's flaky.
IN_PROC_BROWSER_TEST_F(DISABLED_AssistantBrowserTest,
                       ShouldShowSingleErrorOnNetworkDown) {
  tester()->StartAssistantAndWaitForReady();

  ShowAssistantUi();

  ASSERT_TRUE(tester()->IsVisible());

  tester()->DisableFakeS3Server();

  base::RunLoop().RunUntilIdle();

  tester()->SendTextQuery("Is this thing on?");

  tester()->ExpectErrorResponse(
      "Something went wrong. Try again in a few seconds");

  // Make sure no further changes happen to the view hierarchy.
  tester()->ExpectNoChange(base::Seconds(1));

  // This is necessary to prevent a UserInitiatedVoicelessActivity from
  // blocking test harness teardown while we wait on assistant to finish
  // the interaction.
  CloseAssistantUi();
}

}  // namespace ash::assistant
