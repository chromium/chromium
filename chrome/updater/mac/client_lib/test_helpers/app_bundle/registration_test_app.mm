// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Executable portion of a macOS application bundle that exercises features
// of CRURegistration that care about bundle structure.

#import <Foundation/Foundation.h>
#include <dispatch/dispatch.h>

#include <cstdint>
#include <iostream>

#include "base/command_line.h"
#include "base/notreached.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/updater/mac/client_lib/CRURegistration.h"

namespace {

// Exit codes. All error codes defined in CRURegistrationErrorDomain are also
// return values for `register_me`, representing that the operation failed
// due to an error in that domain; other unsuccessful exit codes start at 100
// to avoid colliding with that domain.
//
// In --install_and_register mode, installer errors are a priority over
// registration errors, although the test will keep waiting for the registration
// result after an installer error to detect timeout (likely hang) in such a
// scenario instead.
constexpr int kBadArguments = 100;
constexpr int kAllocFailed = 101;
constexpr int kUnexpectedCRURegistrationError = 102;
constexpr int kTimedOut = 103;
// When multiple consecutive operations are queued up on a CRURegistration, they
// should complete in strict order, which should be observable when the target
// queue is a serial queue (as it is here, where the main queue is used).
// If the "install, then register" test gets a response from the "register"
// operation before it hears from the "install" operation, it returns this
// "disordered sequence" code (as a higher priority than other error codes).
constexpr int kDisorderedSequence = 104;

// Parsed representation of which operation one execution of `register_me`
// should perform.
enum class Mode {
  // No operation flag was (yet) found.
  kUnspecified,
  // Multiple operation flags were found.
  kConflicting,
  // Install the updater. Do not register for updates.
  kInstall,
  // Register for updates. Do not install the updater.
  kRegister,
  // Install the updater and register for updates.
  kInstallAndRegister,
};

NSString* const kAppID = @"org.chromium.CRURegistration.testing.RegisterMe";
constexpr int64_t kTimeoutNanos = 300L * NSEC_PER_SEC;

}  // namespace

// Parse a Mode from command line flags.
Mode ChooseMode() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  Mode mode = Mode::kUnspecified;
  if (command_line->HasSwitch("install")) {
    mode = Mode::kInstall;
  }
  if (command_line->HasSwitch("register")) {
    mode = mode == Mode::kUnspecified ? Mode::kRegister : Mode::kConflicting;
  }
  if (command_line->HasSwitch("install_and_register")) {
    mode = mode == Mode::kUnspecified ? Mode::kInstallAndRegister
                                      : Mode::kConflicting;
  }
  return mode;
}

CRURegistration* InitRegistrationOrDie() {
  CRURegistration* registration =
      [[CRURegistration alloc] initWithAppId:kAppID
                        existenceCheckerPath:[NSBundle mainBundle].bundlePath
                                 targetQueue:dispatch_get_main_queue()];
  if (!registration) {
    std::cout << "InstallUpdater couldn't allocate CRURegistration.\n";
    exit(kAllocFailed);
  }
  return registration;
}

[[noreturn]] void ExitWithError(NSError* error) {
  if (error) {
    std::cout << base::SysNSStringToUTF8([error description]) << std::endl;
    if ([error.domain isEqual:CRURegistrationErrorDomain]) {
      exit(static_cast<int>(error.code));
    } else {
      exit(kUnexpectedCRURegistrationError);
    }
  }
  exit(EXIT_SUCCESS);
}

// Uses CRURegistration to start installing the updater and exits the program
// when it gets a reply. Must be dispatched to the main queue.
void InstallUpdater() {
  @autoreleasepool {
    CRURegistration* registration = InitRegistrationOrDie();
    [registration installUpdaterWithReply:^(NSError* error) {
      ExitWithError(error);
    }];
  }
}

void RegisterWithUpdater() {
  @autoreleasepool {
    CRURegistration* registration = InitRegistrationOrDie();
    [registration registerVersion:@"1.0.0.0"
                            reply:^(NSError* error) {
                              ExitWithError(error);
                            }];
  }
}

void InstallAndRegister() {
  @autoreleasepool {
    CRURegistration* registration = InitRegistrationOrDie();
    __block bool installDone = false;
    __block NSError* installError = nil;
    [registration installUpdaterWithReply:^(NSError* error) {
      std::cout << "Install complete.\n";
      if (error) {
        std::cout << "Install error: "
                  << base::SysNSStringToUTF8([error description]) << std::endl;
      }
      installDone = true;
      installError = error;
    }];
    [registration
        registerVersion:@"2.0.0.0"
                  reply:^(NSError* error) {
                    if (!installDone) {
                      std::cout << "registerVersion:reply: replied before "
                                   "installUpdaterWithReply.\n";
                      if (error) {
                        std::cout
                            << "And, it was an error: "
                            << base::SysNSStringToUTF8([error description])
                            << std::endl;
                      }
                      exit(kDisorderedSequence);
                    }
                    if (installError) {
                      if (error) {
                        std::cout << "installUpdaterWithReply: and "
                                     "registerVersion:reply: both failed. \n";
                        std::cout
                            << "registerVersion's error (received second): "
                            << base::SysNSStringToUTF8([error description])
                            << std::endl;
                      } else {
                        std::cout << "installUpdaterWithReply: failed but "
                                     "registerVersion:reply: subsequently "
                                     "succeeded.\n";
                      }
                      ExitWithError(installError);
                    }
                    if (error) {
                      std::cout
                          << "installUpdaterWithReply: succeeded, but "
                             "registerVersion:reply: subsequently failed:\n";
                      ExitWithError(installError);
                    }
                    exit(0);
                  }];
  }
}

int main(int argc, char** argv) {
  if (!base::CommandLine::Init(argc, argv)) {
    return kBadArguments;
  }
  switch (ChooseMode()) {
    case Mode::kUnspecified:
    case Mode::kConflicting:
      return kBadArguments;
    case Mode::kInstall:
      dispatch_async(dispatch_get_main_queue(), ^{
        InstallUpdater();
      });
      break;
    case Mode::kRegister:
      dispatch_async(dispatch_get_main_queue(), ^{
        RegisterWithUpdater();
      });
      break;
    case Mode::kInstallAndRegister:
      dispatch_async(dispatch_get_main_queue(), ^{
        InstallAndRegister();
      });
      break;
  }
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, kTimeoutNanos),
                 dispatch_get_main_queue(), ^{
                   std::cout << "Timed out after " << kTimeoutNanos
                             << " nanoseconds.\n";
                   exit(kTimedOut);
                 });
  dispatch_main();
  NOTREACHED();
}
