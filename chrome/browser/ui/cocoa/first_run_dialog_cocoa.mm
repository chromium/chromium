// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/first_run_dialog_cocoa.h"

#include "base/apple/bundle_locations.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/current_thread.h"
#import "base/task/single_thread_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/browser_process_impl.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/first_run/first_run_dialog.h"
#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/cocoa/first_run_dialog_controller.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "content/public/common/content_switches.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "url/gurl.h"

@interface FirstRunDialogController (PrivateMethods)
// Show the dialog.
- (void)show;
@end

namespace {

class FirstRunShowBridge : public base::RefCounted<FirstRunShowBridge> {
 public:
  explicit FirstRunShowBridge(FirstRunDialogController* controller);

  void ShowDialog(base::OnceClosure quit_closure);

 private:
  friend class base::RefCounted<FirstRunShowBridge>;

  ~FirstRunShowBridge();

  FirstRunDialogController* controller_;
};

FirstRunShowBridge::FirstRunShowBridge(FirstRunDialogController* controller)
    : controller_(controller) {}

void FirstRunShowBridge::ShowDialog(base::OnceClosure quit_closure) {
  // Proceeding past the modal dialog requires user interaction. Allow nested
  // tasks to run so that signal handlers operate correctly.
  base::CurrentThread::ScopedAllowApplicationTasksInNativeNestedLoop allow;
  [controller_ show];
  std::move(quit_closure).Run();
}

FirstRunShowBridge::~FirstRunShowBridge() = default;

void ShowFirstRunModal() {
  FirstRunDialogController* dialog = [[FirstRunDialogController alloc] init];

  [dialog showWindow:nil];

  // If the dialog asked the user to opt-in for stats and crash reporting,
  // record the decision and enable the crash reporter if appropriate.
  bool consent_given = [dialog isStatsReportingEnabled];
  ChangeMetricsReportingState(consent_given);

  // If selected, set as default browser. Skip in automated tests so that an OS
  // dialog confirming the default browser choice isn't left on screen.
  BOOL make_default_browser =
      [dialog isMakeDefaultBrowserEnabled] &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kTestType);
  if (make_default_browser) {
    bool success = shell_integration::SetAsDefaultBrowser();
    DCHECK(success);
  }
}

}  // namespace

namespace first_run {

void ShowFirstRunDialogCocoa() {
  ShowFirstRunModal();
}

}  // namespace first_run

@implementation FirstRunDialogController {
  FirstRunDialogViewController* __strong _viewController;
}

- (instancetype)init {
  _viewController = [[FirstRunDialogViewController alloc] init];

  // Create the content view controller (and the content view) *before* the
  // window, so that we can find out what the content view's frame is supposed
  // to be for use here.
  NSWindow* window =
      [[NSWindow alloc] initWithContentRect:_viewController.view.frame
                                  styleMask:NSWindowStyleMaskTitled
                                    backing:NSBackingStoreBuffered
                                      defer:YES];
  window.contentView = _viewController.view;
  window.title = [_viewController windowTitle];

  self = [super initWithWindow:window];

  return self;
}

- (IBAction)showWindow:(id)sender {
  // The main MessageLoop has not yet run, but has been spun. If we call
  // -[NSApplication runModalForWindow:] we will hang <http://crbug.com/54248>.
  // Therefore the main MessageLoop is run so things work.

  scoped_refptr<FirstRunShowBridge> bridge(new FirstRunShowBridge(self));
  base::RunLoop run_loop;

  // At this point during startup, ChromeBrowserMain has yet to start the main
  // message loop. Consequently, this run loop will effectively be the main
  // message loop for the duration of the dialog's lifetime. Tell the
  // BrowserProcessImpl how to quit the loop if any of the shutdown signal
  // handlers is received. (The ShutdownDetector posts a task to the UI thread's
  // TaskRunner to begin shutdown upon receiving a SIGTERM.)
  static_cast<BrowserProcessImpl*>(g_browser_process)
      ->SetQuitClosure(base::BindOnce(
          [](base::RunLoop* run_loop) {
            [NSApp abortModal];
            run_loop->Quit();
          },
          &run_loop));

  // Barring a shutdown signal, the run loop will quit when the user closes the
  // first run dialog.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&FirstRunShowBridge::ShowDialog, bridge.get(),
                                run_loop.QuitClosure()));
  run_loop.Run();

  static_cast<BrowserProcessImpl*>(g_browser_process)->ClearQuitClosure();
}

- (void)show {
  NSWindow* win = [self window];

  // Neat weirdness in the below code - the Application menu stays enabled
  // while the window is open but selecting items from it (e.g. Quit) has
  // no effect.  I'm guessing that this is an artifact of us being a
  // background-only application at this stage and displaying a modal
  // window.

  // Display dialog.
  [win center];
  [NSApp runModalForWindow:win];
}

- (BOOL)isStatsReportingEnabled {
  return [_viewController isStatsReportingEnabled];
}

- (BOOL)isMakeDefaultBrowserEnabled {
  return [_viewController isMakeDefaultBrowserEnabled];
}

@end
