// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/direct_launch/direct_launch_manager.h"

#include <windows.h>

#include <stdint.h>

#include <array>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/tracing/trace_event_analyzer.h"
#include "base/test/tracing/trace_test_utils.h"
#include "base/win/scoped_com_initializer.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/startup/launch_mode_recorder.h"
#include "chrome/browser/win/direct_launch/direct_launch_features.h"
#include "chrome/browser/win/direct_launch/direct_launch_histograms.h"
#include "chrome/browser/win/direct_launch/fake_direct_launch_shell.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/util/util_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/win/singleton_hwnd.h"
#include "url/url_constants.h"

namespace direct_launch {

// Runs the manager on this thread, which is a UI thread where COM is
// initialized, like the browser's UI thread. Outside of the anonymous
// namespace, since the manager befriends it to expose its internals.
class DirectLaunchManagerTest : public testing::Test {
 protected:
  DirectLaunchManagerTest() {
    // Enables the feature through a field trial, which checking the feature
    // activates, so that the tests can tell whether the manager checked it.
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        kDirectLaunchManager, {});
  }

  void SetUp() override {
    DirectLaunchManager::SetIsSupportedForTesting(true);
    // The manager only registers from the instance that uses the default user
    // data dir, so make this process that instance, regardless of how the test
    // runner was started.
    chrome::SetUsingDefaultUserDataDirectoryForTesting(true);
  }

  void TearDown() override {
    DestroyManager();
    chrome::SetUsingDefaultUserDataDirectoryForTesting(std::nullopt);
    DirectLaunchManager::SetIsSupportedForTesting(std::nullopt);
  }

  // Creates the manager, which calls into the shell on this thread, so that it
  // only checks for support and registers once this thread runs its tasks.
  void CreateManager() {
    CreateManagerWithShellTaskRunner(
        base::SequencedTaskRunner::GetCurrentDefault());
  }

  // Creates the manager with its calls into the shell on a ThreadPool
  // sequence, as in the browser.
  void CreateManagerWithThreadPoolShellCalls() {
    CreateManagerWithShellTaskRunner(
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
             base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}));
  }

  // Waits until the manager has checked for support, and if it went on to
  // register, until the shell has answered.
  void WaitUntilStarted() {
    ASSERT_TRUE(base::test::RunUntil([this] {
      return !histogram_tester_.GetAllSamples(kDirectLaunchSupportedHistogram)
                  .empty();
    }));
    // The manager posts the registration as soon as it learns of the support.
    FlushShellSequence();
  }

  // Waits for the calls into the shell that the manager has posted so far.
  void FlushShellSequence() { FlushSequence(*shell_task_runner_); }

  // Destroys the manager. It withdraws its registration on the shell sequence,
  // and an answer from there that is still on its way back is dropped.
  void DestroyManager() {
    manager_.reset();
    if (shell_task_runner_) {
      FlushShellSequence();
      FlushSequence(*base::SequencedTaskRunner::GetCurrentDefault());
    }
  }

  // Whether the manager has a way to call into the shell, which it only keeps
  // while it may use direct launch.
  bool CanCallIntoShell() const {
    return !manager_->shell_registrar_.is_null();
  }

  // Makes this process look like a browser that uses a user data dir other
  // than the default one, e.g. one started with --user-data-dir.
  void OverrideUserDataDir() {
    chrome::SetUsingDefaultUserDataDirectoryForTesting(false);
  }

  // Whether the feature has been checked, which counts this client in the
  // feature's experiment population.
  static bool IsFieldTrialActive() {
    base::FieldTrial* trial =
        base::FeatureList::GetFieldTrial(kDirectLaunchManager);
    CHECK(trial);
    return base::FieldTrialList::IsTrialActive(trial->trial_name());
  }

  uint32_t copy_data_format_id() const {
    return manager_->copy_data_format_id_;
  }

  UINT tile_launch_message_id() const {
    return manager_->tile_launch_message_id_;
  }

  // Delivers `message` to the manager, the way gfx::SingletonHwnd does.
  void DeliverMessage(UINT message, LPARAM lparam) {
    manager_->OnWndProc(nullptr, message, 0, lparam);
  }

  // Delivers `copy_data` to the manager as `WM_COPYDATA`.
  void DeliverCopyData(const COPYDATASTRUCT& copy_data) {
    DeliverMessage(WM_COPYDATA, reinterpret_cast<LPARAM>(&copy_data));
  }

  // Tells the manager that the default browser changed, and that `state` is
  // the default browser state of this browser after the change.
  void ChangeDefaultBrowser(shell_integration::DefaultWebClientState state) {
    default_browser_changes_.Notify(state);
  }

  // Whether the manager listens for default browser changes.
  bool IsSubscribedToDefaultBrowserChanges() const {
    return !default_browser_changes_.empty();
  }

  // Renews the registration as if the tile launch message had not been
  // registered, as when `::RegisterWindowMessage()` fails because the session
  // has run out of message ids.
  void RenewRegistrationWithoutTileLaunchMessage() {
    manager_->tile_launch_message_id_ = 0;
    manager_->Register();
  }

  // The command lines that the manager handed on, in order.
  const std::vector<base::CommandLine>& launches() const { return launches_; }

  const base::HistogramTester& histogram_tester() const {
    return histogram_tester_;
  }

 private:
  using DefaultBrowserChangeList = base::RepeatingCallbackList<void(
      shell_integration::DefaultWebClientState)>;

  void CreateManagerWithShellTaskRunner(
      scoped_refptr<base::SequencedTaskRunner> shell_task_runner) {
    shell_task_runner_ = shell_task_runner;
    manager_ = std::make_unique<DirectLaunchManager>(
        RecordLaunches(), SubscribeToDefaultBrowserChanges(),
        std::move(shell_task_runner));
  }

  // Waits for the tasks posted to `task_runner` so far to run, running the
  // tasks of this thread meanwhile.
  static void FlushSequence(base::SequencedTaskRunner& task_runner) {
    base::RunLoop run_loop;
    task_runner.PostTask(FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  // Returns a callback that subscribes the manager to
  // `default_browser_changes_`, the way the browser subscribes it to the
  // changes that `default_browser::DefaultBrowserManager` reports.
  DirectLaunchManager::DefaultBrowserChangeSubscriber
  SubscribeToDefaultBrowserChanges() {
    return base::BindOnce(&DefaultBrowserChangeList::Add,
                          base::Unretained(&default_browser_changes_));
  }

  // Returns a callback that records the command lines handed to it.
  ProcessSingleton::NotificationCallback RecordLaunches() {
    return base::BindLambdaForTesting(
        [this](base::CommandLine command_line,
               const base::FilePath& current_directory) {
          launches_.push_back(std::move(command_line));
          return true;
        });
  }

  // Outlives the threads that `task_environment_` runs.
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  base::win::ScopedCOMInitializer com_initializer_;
  base::HistogramTester histogram_tester_;
  std::vector<base::CommandLine> launches_;
  DefaultBrowserChangeList default_browser_changes_;
  scoped_refptr<base::SequencedTaskRunner> shell_task_runner_;
  std::unique_ptr<DirectLaunchManager> manager_;
};

namespace {

// Returns the launch mode that Launch.Mode2 records for `command_line`.
std::optional<LaunchMode> GetLaunchMode(const base::CommandLine& command_line) {
  base::test::TestFuture<std::optional<LaunchMode>> launch_mode;
  ComputeLaunchMode(command_line, launch_mode.GetCallback());
  return launch_mode.Take();
}

// Registers with a fake of the shell, so that the registration code runs on any
// version of Windows, and never reaches the shell.
class DirectLaunchManagerFakeShellTest : public DirectLaunchManagerTest {
 protected:
  // Creates the manager, and lets it check for support and register.
  void StartManager() {
    CreateManager();
    WaitUntilStarted();
  }

  FakeDirectLaunchShell& shell() { return shell_; }

 private:
  // Created once COM is initialized on this thread, and destroyed after the
  // manager.
  FakeDirectLaunchShell shell_;
};

// The registrations tell the shell where to deliver each kind of launch, and in
// which form, and destroying the manager withdraws both of them.
TEST_F(DirectLaunchManagerFakeShellTest, RegistersUriAndTileLaunches) {
  StartManager();

  EXPECT_EQ(shell().create_count(), 1);
  histogram_tester().ExpectUniqueSample(
      kDirectLaunchUserDataDirOverrideHistogram, false, 1);
  histogram_tester().ExpectUniqueSample(kDirectLaunchSupportedHistogram, true,
                                        1);
  histogram_tester().ExpectUniqueSample(
      kDirectLaunchRegistrationResultHistogram, S_OK, 1);
  EXPECT_TRUE(IsFieldTrialActive());
  EXPECT_TRUE(IsSubscribedToDefaultBrowserChanges());

  // Both kinds of launch go to the window that the manager listens on, in the
  // form that the manager recognizes.
  const HWND hwnd = gfx::SingletonHwnd::GetInstance()->hwnd();
  ASSERT_TRUE(shell().uri_registration().has_value());
  EXPECT_EQ(shell().uri_registration()->target_window, hwnd);
  EXPECT_EQ(shell().uri_registration()->schemes,
            (std::vector<std::wstring>{L"http", L"https"}));
  EXPECT_EQ(shell().uri_registration()->copy_data_format_id,
            copy_data_format_id());
  ASSERT_TRUE(shell().tile_registration().has_value());
  EXPECT_EQ(shell().tile_registration()->target_window, hwnd);
  EXPECT_NE(tile_launch_message_id(), 0u);
  EXPECT_EQ(shell().tile_registration()->message_id, tile_launch_message_id());

  DestroyManager();
  EXPECT_FALSE(shell().uri_registration().has_value());
  EXPECT_FALSE(shell().tile_registration().has_value());
  EXPECT_FALSE(IsSubscribedToDefaultBrowserChanges());
  EXPECT_EQ(shell().invoked_methods(),
            (std::vector<std::wstring>{
                FakeDirectLaunchShell::kRegisterUriLaunch,
                FakeDirectLaunchShell::kRegisterTileLaunch,
                FakeDirectLaunchShell::kUnregisterUriLaunch,
                FakeDirectLaunchShell::kUnregisterTileLaunch,
            }));
}

// The manager checks for support in the background, so it can be destroyed
// before the check completes, which must not lead to a registration.
TEST_F(DirectLaunchManagerFakeShellTest,
       DestroyedBeforeSupportCheckNeverRegisters) {
  CreateManager();
  EXPECT_FALSE(IsSubscribedToDefaultBrowserChanges());
  DestroyManager();

  EXPECT_EQ(shell().create_count(), 0);
  EXPECT_TRUE(shell().invoked_methods().empty());
  histogram_tester().ExpectTotalCount(kDirectLaunchSupportedHistogram, 0);
  EXPECT_FALSE(IsFieldTrialActive());
}

// A browser that uses a user data dir other than the default one, e.g. through
// --user-data-dir, is the process singleton only for that directory. The
// registration is app-wide, so that browser must not claim it, which would
// displace the registration of the browser that the shell actually launches
// into. It must not be counted in the experiment population either.
TEST_F(DirectLaunchManagerFakeShellTest, UserDataDirOverrideKeepsManagerInert) {
  OverrideUserDataDir();
  CreateManager();

  // It does not even check for support.
  EXPECT_FALSE(CanCallIntoShell());
  histogram_tester().ExpectUniqueSample(
      kDirectLaunchUserDataDirOverrideHistogram, true, 1);
  histogram_tester().ExpectTotalCount(kDirectLaunchSupportedHistogram, 0);
  EXPECT_FALSE(IsFieldTrialActive());
  EXPECT_EQ(tile_launch_message_id(), 0u);
  EXPECT_FALSE(IsSubscribedToDefaultBrowserChanges());

  // Nor may it withdraw the registration of the other browser.
  DestroyManager();
  EXPECT_EQ(shell().create_count(), 0);
  EXPECT_TRUE(shell().invoked_methods().empty());
}

TEST_F(DirectLaunchManagerFakeShellTest, NothingRegisteredWhenUnsupported) {
  DirectLaunchManager::SetIsSupportedForTesting(false);
  StartManager();

  histogram_tester().ExpectUniqueSample(kDirectLaunchSupportedHistogram, false,
                                        1);
  // A client that cannot use direct launch is not counted in the experiment
  // population.
  EXPECT_FALSE(IsFieldTrialActive());
  EXPECT_EQ(tile_launch_message_id(), 0u);
  EXPECT_FALSE(IsSubscribedToDefaultBrowserChanges());
  EXPECT_EQ(shell().create_count(), 0);
  histogram_tester().ExpectTotalCount(kDirectLaunchRegistrationResultHistogram,
                                      0);
}

TEST_F(DirectLaunchManagerFakeShellTest, NothingRegisteredWhenDisabled) {
  base::test::ScopedFeatureList disabled_feature_list;
  disabled_feature_list.InitAndDisableFeature(kDirectLaunchManager);
  StartManager();

  histogram_tester().ExpectUniqueSample(kDirectLaunchSupportedHistogram, true,
                                        1);
  EXPECT_EQ(tile_launch_message_id(), 0u);
  EXPECT_FALSE(IsSubscribedToDefaultBrowserChanges());
  EXPECT_EQ(shell().create_count(), 0);
  histogram_tester().ExpectTotalCount(kDirectLaunchRegistrationResultHistogram,
                                      0);

  DestroyManager();
}

// The shell can decline the registration. The manager keeps listening for
// default browser changes nonetheless, so that it can register once this
// browser becomes the default browser.
TEST_F(DirectLaunchManagerFakeShellTest, ShellDeclinesUriLaunchRegistration) {
  shell().SetInvokeResult(FakeDirectLaunchShell::kRegisterUriLaunch,
                          E_ACCESSDENIED);
  StartManager();

  EXPECT_FALSE(shell().uri_registration().has_value());
  EXPECT_FALSE(shell().tile_registration().has_value());
  histogram_tester().ExpectUniqueSample(
      kDirectLaunchRegistrationResultHistogram, E_ACCESSDENIED, 1);
  // Tile launches are not registered without URI launches.
  EXPECT_EQ(shell().invoked_methods(),
            (std::vector<std::wstring>{
                FakeDirectLaunchShell::kRegisterUriLaunch,
            }));

  shell().SetInvokeResult(FakeDirectLaunchShell::kRegisterUriLaunch, S_OK);
  ChangeDefaultBrowser(shell_integration::IS_DEFAULT);
  FlushShellSequence();
  EXPECT_TRUE(shell().uri_registration().has_value());
  EXPECT_TRUE(shell().tile_registration().has_value());
  histogram_tester().ExpectBucketCount(kDirectLaunchRegistrationResultHistogram,
                                       S_OK, 1);

  // Only the registration that the shell accepted is withdrawn.
  DestroyManager();
  EXPECT_EQ(shell().invoked_methods(),
            (std::vector<std::wstring>{
                FakeDirectLaunchShell::kRegisterUriLaunch,
                FakeDirectLaunchShell::kRegisterUriLaunch,
                FakeDirectLaunchShell::kRegisterTileLaunch,
                FakeDirectLaunchShell::kUnregisterUriLaunch,
                FakeDirectLaunchShell::kUnregisterTileLaunch,
            }));
}

// Declining the tile launch registration withdraws the URI launch registration
// that the shell accepted before it, so that the shell is not left half
// registered.
TEST_F(DirectLaunchManagerFakeShellTest,
       DecliningTileLaunchWithdrawsUriLaunch) {
  shell().SetInvokeResult(FakeDirectLaunchShell::kRegisterTileLaunch, E_FAIL);
  StartManager();

  EXPECT_FALSE(shell().uri_registration().has_value());
  EXPECT_FALSE(shell().tile_registration().has_value());
  histogram_tester().ExpectUniqueSample(
      kDirectLaunchRegistrationResultHistogram, E_FAIL, 1);
  const std::vector<std::wstring> expected_methods = {
      FakeDirectLaunchShell::kRegisterUriLaunch,
      FakeDirectLaunchShell::kRegisterTileLaunch,
      FakeDirectLaunchShell::kUnregisterUriLaunch,
  };
  EXPECT_EQ(shell().invoked_methods(), expected_methods);

  // Nothing is left to withdraw.
  DestroyManager();
  EXPECT_EQ(shell().invoked_methods(), expected_methods);
}

// The shell reports a method that it does not support through an out parameter
// rather than by failing it, so a registration that it ignored must not be
// taken for one that it accepted.
TEST_F(DirectLaunchManagerFakeShellTest, UnsupportedUriLaunchIsNotRegistered) {
  shell().SetMethodSupported(FakeDirectLaunchShell::kRegisterUriLaunch, false);
  StartManager();

  histogram_tester().ExpectUniqueSample(
      kDirectLaunchRegistrationResultHistogram, E_NOTIMPL, 1);
  const std::vector<std::wstring> expected_methods = {
      FakeDirectLaunchShell::kRegisterUriLaunch,
  };
  EXPECT_EQ(shell().invoked_methods(), expected_methods);

  // Nothing is left to withdraw.
  DestroyManager();
  EXPECT_EQ(shell().invoked_methods(), expected_methods);
}

TEST_F(DirectLaunchManagerFakeShellTest,
       UnsupportedTileLaunchWithdrawsUriLaunch) {
  shell().SetMethodSupported(FakeDirectLaunchShell::kRegisterTileLaunch, false);
  StartManager();

  EXPECT_FALSE(shell().uri_registration().has_value());
  histogram_tester().ExpectUniqueSample(
      kDirectLaunchRegistrationResultHistogram, E_NOTIMPL, 1);
  const std::vector<std::wstring> expected_methods = {
      FakeDirectLaunchShell::kRegisterUriLaunch,
      FakeDirectLaunchShell::kRegisterTileLaunch,
      FakeDirectLaunchShell::kUnregisterUriLaunch,
  };
  EXPECT_EQ(shell().invoked_methods(), expected_methods);

  // Nothing is left to withdraw.
  DestroyManager();
  EXPECT_EQ(shell().invoked_methods(), expected_methods);
}

// Creating the API can fail even where the OS supports it, e.g. when out of
// memory. That is reported like a declined registration, and leaves nothing to
// withdraw.
TEST_F(DirectLaunchManagerFakeShellTest, ApiCreationFailureIsReported) {
  shell().set_create_result(E_OUTOFMEMORY);
  StartManager();

  EXPECT_EQ(shell().create_count(), 1);
  histogram_tester().ExpectUniqueSample(
      kDirectLaunchRegistrationResultHistogram, E_OUTOFMEMORY, 1);

  DestroyManager();
  EXPECT_TRUE(shell().invoked_methods().empty());
}

// Without a message id to deliver them as, tile launches would reach the
// manager as WM_NULL and be dropped, after the shell had already taken them
// over from the classic launch. So the manager registers URI launches only.
TEST_F(DirectLaunchManagerFakeShellTest,
       TileLaunchesNotRegisteredWithoutMessageId) {
  StartManager();
  RenewRegistrationWithoutTileLaunchMessage();
  FlushShellSequence();

  EXPECT_TRUE(shell().uri_registration().has_value());
  EXPECT_FALSE(shell().tile_registration().has_value());
  histogram_tester().ExpectUniqueSample(
      kDirectLaunchRegistrationResultHistogram, S_OK, 2);

  // URI launches are still delivered.
  shell().LaunchUri(L"https://www.example.com/");
  ASSERT_EQ(launches().size(), 1u);
  EXPECT_THAT(launches()[0].GetArgs(),
              testing::ElementsAre(std::wstring(L"https://www.example.com/")));

  // A message id of 0 matches nothing, `WM_NULL` included, or an unrelated
  // message would be handed on as a tile launch.
  static_assert(WM_NULL == 0, "WM_NULL is the message a 0 id would match.");
  DeliverMessage(WM_NULL, 0);
  EXPECT_EQ(launches().size(), 1u);

  DestroyManager();
  EXPECT_FALSE(shell().uri_registration().has_value());
  EXPECT_EQ(shell().invoked_methods(),
            (std::vector<std::wstring>{
                FakeDirectLaunchShell::kRegisterUriLaunch,
                FakeDirectLaunchShell::kRegisterTileLaunch,
                FakeDirectLaunchShell::kUnregisterUriLaunch,
                FakeDirectLaunchShell::kUnregisterTileLaunch,
                FakeDirectLaunchShell::kRegisterUriLaunch,
                FakeDirectLaunchShell::kUnregisterUriLaunch,
            }));
}

// The shell only honors the registration while this browser is the default
// browser, so the registration is renewed when the default browser changes,
// which replaces the registration held rather than adding to it.
TEST_F(DirectLaunchManagerFakeShellTest,
       DefaultBrowserChangeRenewsRegistration) {
  StartManager();
  ChangeDefaultBrowser(shell_integration::IS_DEFAULT);
  FlushShellSequence();

  EXPECT_TRUE(shell().uri_registration().has_value());
  EXPECT_TRUE(shell().tile_registration().has_value());
  EXPECT_EQ(shell().create_count(), 2);
  EXPECT_EQ(shell().invoked_methods(),
            (std::vector<std::wstring>{
                FakeDirectLaunchShell::kRegisterUriLaunch,
                FakeDirectLaunchShell::kRegisterTileLaunch,
                FakeDirectLaunchShell::kUnregisterUriLaunch,
                FakeDirectLaunchShell::kUnregisterTileLaunch,
                FakeDirectLaunchShell::kRegisterUriLaunch,
                FakeDirectLaunchShell::kRegisterTileLaunch,
            }));
  histogram_tester().ExpectUniqueSample(
      kDirectLaunchRegistrationResultHistogram, S_OK, 2);

  DestroyManager();
  EXPECT_FALSE(shell().uri_registration().has_value());
  EXPECT_FALSE(shell().tile_registration().has_value());
}

// The shell ignores the registration while this browser is not the default
// browser, so the registration is not renewed when another browser, or another
// install mode of this one, becomes the default browser. It is renewed when it
// is unknown which browser is the default browser.
TEST_F(DirectLaunchManagerFakeShellTest, RenewedUnlessNotDefaultBrowser) {
  StartManager();
  histogram_tester().ExpectTotalCount(kDirectLaunchRegistrationResultHistogram,
                                      1);

  ChangeDefaultBrowser(shell_integration::NOT_DEFAULT);
  ChangeDefaultBrowser(shell_integration::OTHER_MODE_IS_DEFAULT);
  FlushShellSequence();
  EXPECT_EQ(shell().create_count(), 1);
  histogram_tester().ExpectTotalCount(kDirectLaunchRegistrationResultHistogram,
                                      1);

  ChangeDefaultBrowser(shell_integration::UNKNOWN_DEFAULT);
  FlushShellSequence();
  EXPECT_EQ(shell().create_count(), 2);
  EXPECT_TRUE(shell().uri_registration().has_value());
  EXPECT_TRUE(shell().tile_registration().has_value());
  histogram_tester().ExpectUniqueSample(
      kDirectLaunchRegistrationResultHistogram, S_OK, 2);
}

TEST_F(DirectLaunchManagerFakeShellTest, ShellLaunchesAreHandedOn) {
  StartManager();

  shell().LaunchUri(L"https://www.example.com/d");
  ASSERT_EQ(launches().size(), 1u);
  EXPECT_THAT(launches()[0].GetArgs(),
              testing::ElementsAre(std::wstring(L"https://www.example.com/d")));

  shell().LaunchTile();
  ASSERT_EQ(launches().size(), 2u);
  EXPECT_TRUE(launches()[1].GetArgs().empty());

  // The URI launch is counted by its accepted payload, and the tile launch,
  // which carries no payload, on its own.
  histogram_tester().ExpectUniqueSample(kDirectLaunchPayloadResultHistogram,
                                        DirectLaunchPayloadResult::kAccepted,
                                        1);
  histogram_tester().ExpectUniqueSample(kDirectLaunchTileLaunchHistogram, true,
                                        1);
}

// The URI is handed on through `base::CommandLine::FromString()`, which
// re-parses it with `::CommandLineToArgvW()`. Without --single-argument, that
// splits the URI on whitespace and strips quotes, so an OAuth authorization
// URL, whose "scope" is a space-delimited list, would lose every query
// parameter after the first space, including "redirect_uri" and "state". See
// crbug.com/937179.
TEST_F(DirectLaunchManagerFakeShellTest, UriLaunchPreservesUriVerbatim) {
  StartManager();

  static constexpr const wchar_t* kUris[] = {
      L"https://www.example.com/simple",
      // Unencoded spaces, as produced by an OAuth "scope" list.
      L"https://auth.example.com/oauth/auth?client_id=app&response_type=code"
      L"&scope=openid profile offline_access"
      L"&redirect_uri=http://localhost:51763/&state=abc123",
      // Unencoded quotes.
      L"https://auth.example.com/oauth/auth?state=\"abc\"&redirect_uri=x",
      // A space followed by a token that otherwise looks like a switch.
      L"https://auth.example.com/oauth/auth?scope=openid /profile&state=abc",
  };

  for (const wchar_t* uri : kUris) {
    SCOPED_TRACE(uri);
    shell().LaunchUri(uri);
    ASSERT_FALSE(launches().empty());
    const base::CommandLine& launch = launches().back();

    EXPECT_THAT(launch.GetArgs(), testing::ElementsAre(std::wstring(uri)));
    // --single-argument must be consumed as a parsing directive rather than
    // survive as a browser switch, and no part of the URI may be interpreted
    // as one either, or a URI containing a space could inject switches into
    // the browser process. The only switch is the source tag that direct
    // launch adds itself.
    EXPECT_THAT(launch.GetSwitches(),
                testing::ElementsAre(
                    testing::Pair(switches::kSourceAppId, std::wstring())));
    EXPECT_EQ(launch.GetProgram().value(), installer::kChromeExe);

    // The manager derives this format from
    // `base::CommandLine::GetCommandLineStringForShell()`. Pin the literal
    // here instead of calling the same helper, so that the command line that
    // the shell builds for the registered ProgID open verb and the one that
    // direct launch builds cannot silently drift apart.
    const std::wstring shell_string =
        base::StrCat({installer::kChromeExe, L" --single-argument ", uri});
    EXPECT_EQ(launch.GetArgs(),
              base::CommandLine::FromString(shell_string).GetArgs());
  }
  EXPECT_EQ(launches().size(), std::size(kUris));
}

// A tile launch has no URI, which must neither produce a stray empty argument
// nor leave --single-argument behind as a switch.
TEST_F(DirectLaunchManagerFakeShellTest, TileLaunchHasNoArguments) {
  StartManager();

  shell().LaunchTile();

  ASSERT_EQ(launches().size(), 1u);
  EXPECT_TRUE(launches()[0].GetArgs().empty());
  EXPECT_THAT(launches()[0].GetSwitches(),
              testing::ElementsAre(
                  testing::Pair(switches::kSourceAppId, std::wstring())));
}

// Launch.Mode2 attributes a launch that arrives through the process singleton
// to how the launching process was started, which a rendezvous forwards as
// --source-app-id or --source-shortcut, and failing that, to how the running
// browser was started. A direct launch has no launching process, so it must
// carry its own tag, or a link click could be counted as, e.g., a launch from
// the taskbar shortcut that started the browser. Tagged, its launch mode does
// not depend on how this test process was started either.
TEST_F(DirectLaunchManagerFakeShellTest, LaunchesAreAttributedToTheShell) {
  StartManager();

  // A URI launch counts like the protocol handler launch that it replaces.
  shell().LaunchUri(L"https://www.example.com/");
  ASSERT_EQ(launches().size(), 1u);
  EXPECT_TRUE(launches()[0].HasSwitch(switches::kSourceAppId));
  EXPECT_TRUE(launches()[0].HasSingleArgumentSwitch());
  EXPECT_EQ(GetLaunchMode(launches()[0]), LaunchMode::kProtocolHandler);

  // A tile launch counts as a launch through the browser's app id, which
  // Launch.Mode2 attributes to the taskbar or the Start menu.
  shell().LaunchTile();
  ASSERT_EQ(launches().size(), 2u);
  EXPECT_TRUE(launches()[1].HasSwitch(switches::kSourceAppId));
  EXPECT_EQ(GetLaunchMode(launches()[1]), LaunchMode::kWithAppId);
}

// The `WM_COPYDATA` payload is dictated by another process and is not
// guaranteed to be NUL-terminated, so the URI must be bounded by the
// sender-declared `cbData`. Scanning for a terminator instead would read past
// the end of the buffer and append garbage to the URI.
TEST_F(DirectLaunchManagerFakeShellTest, UnterminatedPayloadIsBoundedByCbData) {
  StartManager();

  const std::wstring uri = L"https://www.example.com/a";
  // Data past the declared length. Seeing any of it in the URI means the
  // manager read past `cbData`.
  std::wstring buffer = uri + L"SHOULD-NOT-BE-READ";
  DeliverCopyData({
      .dwData = copy_data_format_id(),
      .cbData = static_cast<DWORD>(uri.size() * sizeof(wchar_t)),
      .lpData = buffer.data(),
  });

  ASSERT_EQ(launches().size(), 1u);
  EXPECT_THAT(launches()[0].GetArgs(), testing::ElementsAre(uri));
}

// A NUL-terminated payload is the expected shape. Neither the terminator nor
// any padding beyond it may leak into the URI.
TEST_F(DirectLaunchManagerFakeShellTest, TerminatedPayloadIsTruncatedAtNul) {
  StartManager();

  const std::wstring uri = L"https://www.example.com/b";
  std::wstring buffer = uri;
  buffer.push_back(L'\0');
  buffer.append(L"PADDING");
  buffer.push_back(L'\0');
  DeliverCopyData({
      .dwData = copy_data_format_id(),
      .cbData = static_cast<DWORD>(buffer.size() * sizeof(wchar_t)),
      .lpData = buffer.data(),
  });

  ASSERT_EQ(launches().size(), 1u);
  EXPECT_THAT(launches()[0].GetArgs(), testing::ElementsAre(uri));
}

// gfx::SingletonHwnd is shared, so unrelated senders can deliver `WM_COPYDATA`
// to it. Malformed payloads must be rejected rather than read as a launch URI.
TEST_F(DirectLaunchManagerFakeShellTest, MalformedPayloadIsRejected) {
  StartManager();
  const uint32_t format_id = copy_data_format_id();
  wchar_t uri[] = L"https://www.example.com/c";
  wchar_t empty[] = L"";

  // No COPYDATASTRUCT at all.
  DeliverMessage(WM_COPYDATA, 0);

  // A null buffer with a nonzero declared length.
  DeliverCopyData({.dwData = format_id, .cbData = sizeof(uri)});

  // A zero-length buffer.
  DeliverCopyData({.dwData = format_id, .cbData = 0, .lpData = uri});

  // A length that is not a whole number of `wchar_t`.
  DeliverCopyData(
      {.dwData = format_id, .cbData = sizeof(uri) - 1, .lpData = uri});

  // A buffer that does not start on a `wchar_t` boundary, although its length
  // is a whole number of them. Its contents would otherwise make for a URI.
  alignas(wchar_t) std::array<char, 2 * sizeof(wchar_t) + 1> unaligned;
  unaligned.fill('a');
  DeliverCopyData({.dwData = format_id,
                   .cbData = 2 * sizeof(wchar_t),
                   .lpData = &unaligned[1]});

  // A declared length far beyond the longest URL Chrome handles. Rejecting on
  // `cbData` alone means that no allocation is attempted, and that the much
  // smaller real buffer is never read.
  DeliverCopyData({.dwData = format_id, .cbData = 0xFFFFFFF0, .lpData = uri});

  // A URI launch without a URI, which must not be handed on as a tile launch.
  DeliverCopyData(
      {.dwData = format_id, .cbData = sizeof(empty), .lpData = empty});

  // A well-formed URI that does not carry the registered copy data format id
  // belongs to some other sender on the shared window, not to direct launch.
  DeliverCopyData({.dwData = format_id ^ 1u,
                   .cbData = sizeof(uri) - sizeof(wchar_t),
                   .lpData = uri});

  // A sender that never sets `dwData` leaves it at 0, which must not be
  // mistaken for the registered copy data format id either.
  DeliverCopyData({.cbData = sizeof(uri) - sizeof(wchar_t), .lpData = uri});

  EXPECT_TRUE(launches().empty());
  histogram_tester().ExpectTotalCount(kDirectLaunchTileLaunchHistogram, 0);

  // Each rejection is reported with its reason, so that a change of the
  // shell's payload format is diagnosable rather than looking like the shell
  // sending nothing at all. Traffic from other senders is not direct launch
  // traffic, and is not reported at all.
  histogram_tester().ExpectBucketCount(kDirectLaunchPayloadResultHistogram,
                                       DirectLaunchPayloadResult::kNullBuffer,
                                       1);
  histogram_tester().ExpectBucketCount(kDirectLaunchPayloadResultHistogram,
                                       DirectLaunchPayloadResult::kTooSmall, 1);
  histogram_tester().ExpectBucketCount(kDirectLaunchPayloadResultHistogram,
                                       DirectLaunchPayloadResult::kMisaligned,
                                       2);
  histogram_tester().ExpectBucketCount(kDirectLaunchPayloadResultHistogram,
                                       DirectLaunchPayloadResult::kOversized,
                                       1);
  histogram_tester().ExpectBucketCount(kDirectLaunchPayloadResultHistogram,
                                       DirectLaunchPayloadResult::kEmpty, 1);
  histogram_tester().ExpectTotalCount(kDirectLaunchPayloadResultHistogram, 6);
}

// Each rejection is also traced with its reason and the declared size of the
// payload, so that it can be lined up against the shell's activity in the same
// capture.
TEST_F(DirectLaunchManagerFakeShellTest, RejectedPayloadIsTraced) {
  base::test::TracingEnvironment tracing_environment;
  StartManager();
  const uint32_t format_id = copy_data_format_id();
  wchar_t uri[] = L"https://www.example.com/e";
  wchar_t empty[] = L"";
  const auto payloads = std::to_array<COPYDATASTRUCT>({
      {.dwData = format_id, .cbData = sizeof(uri), .lpData = nullptr},
      {.dwData = format_id, .cbData = 0, .lpData = uri},
      {.dwData = format_id, .cbData = sizeof(uri) - 1, .lpData = uri},
      {.dwData = format_id, .cbData = 0xFFFFFFF0, .lpData = uri},
      {.dwData = format_id, .cbData = sizeof(empty), .lpData = empty},
  });

  trace_analyzer::Start("startup");
  for (const COPYDATASTRUCT& payload : payloads) {
    DeliverCopyData(payload);
  }
  std::unique_ptr<trace_analyzer::TraceAnalyzer> analyzer =
      trace_analyzer::Stop();
  ASSERT_TRUE(analyzer);

  trace_analyzer::TraceEventVector events;
  analyzer->FindEvents(trace_analyzer::Query::EventNameIs(
                           "DirectLaunchManager::OnWndProc PayloadRejected"),
                       &events);
  EXPECT_EQ(events.size(), payloads.size());
  // Keyed by reason, since the order of the events is not under test.
  base::flat_map<std::string, double> sizes_by_reason;
  for (const trace_analyzer::TraceEvent* event : events) {
    ASSERT_TRUE(event->HasStringArg("reason"));
    ASSERT_TRUE(event->HasNumberArg("cbData"));
    sizes_by_reason.emplace(event->GetKnownArgAsString("reason"),
                            event->GetKnownArgAsDouble("cbData"));
  }
  EXPECT_EQ(sizes_by_reason, (base::flat_map<std::string, double>{
                                 {"NullBuffer", sizeof(uri)},
                                 {"TooSmall", 0},
                                 {"Misaligned", sizeof(uri) - 1},
                                 {"Oversized", 0xFFFFFFF0},
                                 {"Empty", sizeof(empty)},
                             }));
}

// The copy data format id is the only thing that separates direct launch
// traffic from every other `WM_COPYDATA` sender on the shared
// gfx::SingletonHwnd, so it must not be a value an unrelated sender lands on
// without meaning to. 0 is exactly that value: it is what a sender that never
// sets `dwData` leaves behind.
TEST_F(DirectLaunchManagerFakeShellTest, CopyDataFormatIdIsNotZero) {
  CreateManager();
  EXPECT_NE(copy_data_format_id(), 0u);
}

// The tile launch message id is compared against every message arriving on the
// shared gfx::SingletonHwnd. Until the manager registers one it is 0, and 0
// must match nothing, `WM_NULL` included, or an unrelated message would be
// handed on as a tile launch.
TEST_F(DirectLaunchManagerFakeShellTest,
       UnregisteredTileLaunchMessageIdMatchesNothing) {
  CreateManager();
  ASSERT_EQ(tile_launch_message_id(), 0u);

  DeliverMessage(WM_NULL, 0);

  EXPECT_TRUE(launches().empty());
  histogram_tester().ExpectTotalCount(kDirectLaunchTileLaunchHistogram, 0);
}

// The payload cap must accept the longest URL Chrome handles and reject
// anything longer, so that a sender cannot force an unbounded allocation by
// declaring an arbitrary `cbData`.
TEST_F(DirectLaunchManagerFakeShellTest, PayloadSizeCapBoundary) {
  StartManager();

  // Sized one `wchar_t` past the cap, so that the payload past the cap below
  // would read initialized memory rather than run off the end if it were
  // wrongly accepted.
  std::wstring buffer(url::kMaxURLChars + 1, L'a');

  // One `wchar_t` past the cap: rejected. Checked first, so that the
  // acceptance below is unambiguous.
  DeliverCopyData({
      .dwData = copy_data_format_id(),
      .cbData = static_cast<DWORD>((url::kMaxURLChars + 1) * sizeof(wchar_t)),
      .lpData = buffer.data(),
  });
  EXPECT_TRUE(launches().empty());
  histogram_tester().ExpectUniqueSample(kDirectLaunchPayloadResultHistogram,
                                        DirectLaunchPayloadResult::kOversized,
                                        1);

  // Exactly at the cap: accepted.
  DeliverCopyData({
      .dwData = copy_data_format_id(),
      .cbData = static_cast<DWORD>(url::kMaxURLChars * sizeof(wchar_t)),
      .lpData = buffer.data(),
  });
  ASSERT_EQ(launches().size(), 1u);
  ASSERT_EQ(launches()[0].GetArgs().size(), 1u);
  EXPECT_EQ(launches()[0].GetArgs()[0].size(), url::kMaxURLChars);
  histogram_tester().ExpectBucketCount(kDirectLaunchPayloadResultHistogram,
                                       DirectLaunchPayloadResult::kAccepted, 1);
  histogram_tester().ExpectTotalCount(kDirectLaunchPayloadResultHistogram, 2);
}

// The manager builds the command line of a launch from the one that the shell
// runs for the ProgID open verb by putting the URI in place of the "%1"
// placeholder, which it relies on being at the very end of that command line.
TEST(DirectLaunchShellCommandLineTest, EndsInPlaceholder) {
  EXPECT_TRUE(base::CommandLine(base::FilePath(installer::kChromeExe))
                  .GetCommandLineStringForShell()
                  .ends_with(L"%1"));
}

// Registers with the shell for real, so this only runs on request. Beyond a
// version of Windows that supports direct launch, it needs a test binary that
// embeds the Limited Access Feature identity that the API token is bound to,
// as chrome.exe does (see chrome_exe.rc). And since the registration is
// app-wide, concurrent runs would interfere with one another.
TEST_F(DirectLaunchManagerTest, MANUAL_RegistersWithShell) {
  DirectLaunchManager::SetIsSupportedForTesting(std::nullopt);
  CreateManagerWithThreadPoolShellCalls();
  WaitUntilStarted();
  if (histogram_tester().GetBucketCount(kDirectLaunchSupportedHistogram,
                                        true) == 0) {
    GTEST_SKIP() << "Direct launch is unavailable to this binary on this OS.";
  }

  histogram_tester().ExpectUniqueSample(
      kDirectLaunchRegistrationResultHistogram, S_OK, 1);
}

}  // namespace

}  // namespace direct_launch
