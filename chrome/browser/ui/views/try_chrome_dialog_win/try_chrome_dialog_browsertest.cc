// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/common/chrome_result_codes.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::InvokeWithoutArgs;
using ::testing::Range;

// Unfortunately, this needs to be Windows only for now. Even though this test
// is meant to exercise code that is for Windows only, it is a good general
// canary in the coal mine for problems related to early shutdown (aborted
// startup). Sadly, it times out on platforms other than Windows, so I can't
// enable it for those platforms at the moment. I hope one day our test harness
// will be improved to support this so we can get coverage on other platforms.
// See http://crbug.com/45115 for details.
#if defined(OS_WIN)
#include "chrome/browser/ui/views/try_chrome_dialog_win/try_chrome_dialog.h"

#include "ui/aura/window.h"
#include "ui/gfx/win/singleton_hwnd.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_win.h"
#include "ui/views/widget/widget.h"

// By passing kTryChromeAgain with a magic value > 10000 we cause Chrome
// to exit fairly early.
// Quickly exiting Chrome (regardless of this particular flag -- it
// doesn't do anything other than cause Chrome to quit on startup on
// non-Windows) was a cause of crashes (see bug 34799 for example) so
// this is a useful test of the startup/quick-shutdown cycle.
class TryChromeDialogBrowserTest : public InProcessBrowserTest {
 public:
  TryChromeDialogBrowserTest() {
    set_expected_exit_code(chrome::RESULT_CODE_NORMAL_EXIT_CANCEL);
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kTryChromeAgain, "10001");
  }
};

// Note to Sheriffs: This test (as you can read about above) simply causes
// Chrome to shutdown early, and, as such, has proven to be pretty good at
// finding problems related to shutdown. Sheriff, before marking this test as
// disabled, please consider that this test is meant to catch when people
// introduce changes that crash Chrome during shutdown and disabling this test
// and moving on removes a safeguard meant to avoid an even bigger thorny mess
// to untangle much later down the line. Disabling the test also means that the
// people who get blamed are not the ones that introduced the crash (in other
// words, don't shoot the messenger). So, please help us avoid additional
// shutdown crashes from creeping in, by doing the following:
// Run chrome.exe --try-chrome-again=10001. This is all that the test does and
// should be enough to trigger the failure. If it is a crash (most likely) then
// look at the callstack and see if any of the components have been touched
// recently. Look at recent changes to startup, such as any change to
// ChromeBrowserMainParts, specifically PreCreateThreadsImpl and see if someone
// has been reordering code blocks for startup. Try reverting any suspicious
// changes to see if it affects the test. History shows that waiting until later
// only makes the problem worse.
IN_PROC_BROWSER_TEST_F(TryChromeDialogBrowserTest, ToastCrasher) {}

// A test fixture that provides a convenience method for synchronously showing
// the TryChromeDialog and a mock delegate suitable for use in testing various
// functionality.
class TryChromeDialogBrowserTestBase : public InProcessBrowserTest {
 public:
  // Breaks ShowDialogSync() out of its modal run loop.
  void QuitModalLoop() {
    if (quit_closure_)
      quit_closure_.Run();
  }

 protected:
  class MockDelegate : public TryChromeDialog::Delegate {
   public:
    MOCK_METHOD1(SetToastLocation,
                 void(installer::ExperimentMetrics::ToastLocation));
    MOCK_METHOD1(SetExperimentState, void(installer::ExperimentMetrics::State));
    MOCK_METHOD0(InteractionComplete, void());
  };

  explicit TryChromeDialogBrowserTestBase(int group = 0) : group_(group) {
    // Configure the delegate to exit the modal loop when the dialog is shown
    // and again when it has finished its work.
    ON_CALL(delegate_, SetToastLocation(_))
        .WillByDefault(InvokeWithoutArgs(
            this, &TryChromeDialogBrowserTestBase::QuitModalLoop));
    ON_CALL(delegate_, InteractionComplete())
        .WillByDefault(InvokeWithoutArgs(
            this, &TryChromeDialogBrowserTestBase::QuitModalLoop));
  }

  // content:BrowserTestBase:
  void SetUpOnMainThread() override {
    dialog_ = base::WrapUnique(new TryChromeDialog(group_, &delegate_));
  }
  void TearDownInProcessBrowserTestFixture() override { dialog_.reset(); }

  MockDelegate& delegate() { return delegate_; }

  // Returns the result of showing the dialog.
  TryChromeDialog::Result result() const { return dialog_->result(); }

  // Returns the HWND housing the dialog.
  HWND dialog_hwnd() {
    return dialog_->widget()
        ->GetNativeWindow()
        ->GetHost()
        ->GetAcceleratedWidget();
  }

  // Fires off the task(s) to show the dialog, breaking out of the message loop
  // once the dialog has been shown.
  void ShowDialogSync() {
    dialog_->ShowDialogAsync();
    RunUntilQuit();
  }

  // Posts a task to the UI thread to simulate a rendezvous from another browser
  // process.
  void PostRendezvousTask() {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&TryChromeDialog::OnProcessNotification,
                                  base::Unretained(dialog_.get())));
  }

  // Runs a loop until it is quit via either the dialog being shown (by way of
  // the default action on the mock delegate's SetLayoutManager) or the
  // interaction with the dialog completing (by way of the default action on the
  // mock delegate's InteractionComplete).
  void RunUntilQuit() {
    base::RunLoop run_loop;

    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
    quit_closure_.Reset();
  }

 private:
  const int group_;
  ::testing::NiceMock<MockDelegate> delegate_;
  std::unique_ptr<TryChromeDialog> dialog_;
  base::Closure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(TryChromeDialogBrowserTestBase);
};

// Showing the dialog then closing it via WM_CLOSE should not launch the
// browser.
IN_PROC_BROWSER_TEST_F(TryChromeDialogBrowserTestBase, ShowAndCloseNotNow) {
  // Open the toast, expecting that the delegate gets the location.
  EXPECT_CALL(delegate(), SetToastLocation(_));
  ShowDialogSync();
  ::testing::Mock::VerifyAndClearExpectations(&delegate());

  // Now close it, verifying that the delegate is invoked as appropriate.
  {
    InSequence delegate_sequence;
    EXPECT_CALL(delegate(),
                SetExperimentState(installer::ExperimentMetrics::kOtherClose));
    EXPECT_CALL(delegate(), InteractionComplete());
  }

  // Close the popup from the outside.
  ::PostMessage(dialog_hwnd(), WM_CLOSE, 0, 0);

  RunUntilQuit();
  ::testing::Mock::VerifyAndClearExpectations(&delegate());

  // Since the dialog was closed by external factors, the overall result should
  // be to exit the browser process.
  EXPECT_THAT(result(), Eq(TryChromeDialog::NOT_NOW));
}

// Showing the dialog then receiving a WM_ENDSESSION should not launch the
// browser.
IN_PROC_BROWSER_TEST_F(TryChromeDialogBrowserTestBase, ShowAndEndSession) {
  // Open the toast, expecting that the delegate gets the location.
  EXPECT_CALL(delegate(), SetToastLocation(_));
  ShowDialogSync();
  ::testing::Mock::VerifyAndClearExpectations(&delegate());

  // Expect that the state is moved to UserLogOff without completing the
  // interaction.
  EXPECT_CALL(delegate(),
              SetExperimentState(installer::ExperimentMetrics::kUserLogOff))
      .WillOnce(InvokeWithoutArgs(
          this, &TryChromeDialogBrowserTestBase::QuitModalLoop));
  EXPECT_CALL(delegate(), InteractionComplete()).Times(0);

  // Send a WM_ENDSESSION to the singleton hwnd to simulate user logoff.
  ::PostMessage(gfx::SingletonHwnd::GetInstance()->hwnd(), WM_ENDSESSION, TRUE,
                0);
  RunUntilQuit();
  ::testing::Mock::VerifyAndClearExpectations(&delegate());

  // The dialog is still open, so the result remains in its initial state.
  EXPECT_THAT(result(), Eq(TryChromeDialog::NOT_NOW));
}

// Receiving a WM_ENDSESSION before the dialog is even shown should not launch
// the browser.
IN_PROC_BROWSER_TEST_F(TryChromeDialogBrowserTestBase, EarlyEndSession) {
  // Send a WM_ENDSESSION to the singleton hwnd to simulate user logoff.
  ::PostMessage(gfx::SingletonHwnd::GetInstance()->hwnd(), WM_ENDSESSION, TRUE,
                0);

  // Expect that the state is moved to UserLogOff without the toast being shown
  // or completing the interaction.
  EXPECT_CALL(delegate(), SetToastLocation(_)).Times(0);
  EXPECT_CALL(delegate(),
              SetExperimentState(installer::ExperimentMetrics::kUserLogOff))
      .WillOnce(InvokeWithoutArgs(
          this, &TryChromeDialogBrowserTestBase::QuitModalLoop));
  EXPECT_CALL(delegate(), InteractionComplete()).Times(0);
  ShowDialogSync();
  ::testing::Mock::VerifyAndClearExpectations(&delegate());

  // The dialog is still open, so the result remains in its initial state.
  EXPECT_THAT(result(), Eq(TryChromeDialog::NOT_NOW));
}

// Showing the dialog then receiving a rendezvous should suppress the initial
// launch.
IN_PROC_BROWSER_TEST_F(TryChromeDialogBrowserTestBase, ShowAndRendezvous) {
  // Open the toast, expecting that the delegate gets the location.
  EXPECT_CALL(delegate(), SetToastLocation(_));
  ShowDialogSync();
  ::testing::Mock::VerifyAndClearExpectations(&delegate());

  // Expect that the state is moved to OtherLaunch and the interaction
  // completes with OPEN_CHROME_DEFER.
  {
    InSequence delegate_sequence;
    EXPECT_CALL(delegate(),
                SetExperimentState(installer::ExperimentMetrics::kOtherLaunch));
    EXPECT_CALL(delegate(), InteractionComplete());
  }

  // Queue up the notification from the other browser process.
  PostRendezvousTask();
  RunUntilQuit();
  ::testing::Mock::VerifyAndClearExpectations(&delegate());

  EXPECT_THAT(result(), Eq(TryChromeDialog::OPEN_CHROME_DEFER));
}

// Receiving a rendezvous before the dialog is even shown should not launch
// the browser.
IN_PROC_BROWSER_TEST_F(TryChromeDialogBrowserTestBase, EarlyRendezvous) {
  // Queue up the notification from the other browser process.
  PostRendezvousTask();

  // Expect that the state is moved to OtherLaunch and that the interaction
  // completes without the toast being shown.
  {
    InSequence delegate_sequence;
    EXPECT_CALL(delegate(), SetToastLocation(_)).Times(0);
    EXPECT_CALL(delegate(),
                SetExperimentState(installer::ExperimentMetrics::kOtherLaunch));
    EXPECT_CALL(delegate(), InteractionComplete());
  }
  ShowDialogSync();
  ::testing::Mock::VerifyAndClearExpectations(&delegate());

  EXPECT_THAT(result(), Eq(TryChromeDialog::OPEN_CHROME_DEFER));
}

// Test harness to display the TryChromeDialog for testing. The test parameter
// is the group number to be evaluated.
class TryChromeDialogTest
    : public SupportsTestDialog<TryChromeDialogBrowserTestBase>,
      public ::testing::WithParamInterface<int> {
 protected:
  TryChromeDialogTest()
      : SupportsTestDialog<TryChromeDialogBrowserTestBase>(GetParam()) {}

  // TestBrowserDialog:
  void ShowUi(const std::string& name) override { ShowDialogSync(); }
  std::string GetNonDialogName() override {
    // This class tests a non-dialog widget with the following name.
    return "TryChromeDialog";
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TryChromeDialogTest);
};

IN_PROC_BROWSER_TEST_P(TryChromeDialogTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(
    Variations,
    TryChromeDialogTest,
    ::testing::Range(
        0,
        static_cast<int>(installer::ExperimentMetrics::kHoldbackGroup)));

#endif  // defined(OS_WIN)
