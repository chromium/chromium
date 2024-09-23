// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_FUZZING_IN_PROCESS_FUZZER_H_
#define CHROME_TEST_FUZZING_IN_PROCESS_FUZZER_H_

#include <optional>

#include "chrome/test/base/in_process_browser_test.h"

enum class RunLoopTimeoutBehavior {
  // Default behavior that doesn't alter the current way run loop internal
  // mechanism handles timeouts.
  kDefault,
  // Continues the normal execution after the call to RunLoop::Run. This
  // basically ignores the timeouts.
  kContinue,
  // Calls InProcessFuzzer::DeclareInfiniteLoop. This will still run the fuzz
  // case as `kContinue` would until the Fuzz method returns.
  kDeclareInfiniteLoop,
};

struct InProcessFuzzerOptions {
  // The behavior to be set when a run loop times out.
  RunLoopTimeoutBehavior run_loop_timeout_behavior =
      RunLoopTimeoutBehavior::kDefault;

  // Sets the timeout for the "Fuzz" method to complete.
  std::optional<base::TimeDelta> run_loop_timeout = std::nullopt;
};

// In-process fuzz test.
//
// This is equivalent to a browser test, in that the entire browser
// environment is available for your use, and you can do rich things that
// require the whole browser infrastructure.
//
// The 'Fuzz' method will be called repeatedly, and you just have to
// implement something sensible there to explore parts of Chrome's
// attack surface.
//
// Register your subclass with REGISTER_IN_PROCESS_FUZZER. There can only
// be one per executable.
//
// Different fuzz frameworks might run this in different ways.
// For instance,
// * libfuzzer runs this in a multi-process Chrome browser_test
//   environment.
// * centipede runs it in single-process browser_test mode (currently),
//   with an external fuzz co-ordinator running multiple instances
//   of Chrome.
// * in the future, snapshot fuzzers might pause a VM and resume
//   clones of it (to ensure a cleaner state for each iteration)
// To the extent possible, you should write your fuzzer to be
// implementation-independent and semantically express what
// should happen during such fuzzing of the whole browser.
class InProcessFuzzer : virtual public InProcessBrowserTest {
 public:
  // Called by the main function to create this class.
  // This is called prior to all the normal browser test setup,
  // so don't do anything important in your constructor.
  // Furthermore, this will be re-run even for child Chromium processes.
  // NOLINTNEXTLINE(runtime/explicit)
  InProcessFuzzer(InProcessFuzzerOptions options = {});
  ~InProcessFuzzer() override;

  // Called by the main function to run this fuzzer, after the browser_test
  // equivalent infrastructure has been set up.
  void Run(const std::vector<std::string>& libfuzzer_command_line);

  // If you override this, it's essential you call the superclass method.
  void SetUpOnMainThread() override;
  void RunTestOnMainThread() override;
  void TestBody() override {}
  void SetUp() override;

  friend int fuzz_callback(const uint8_t* data, size_t size);

  // Override if you want to pass particular command line arguments to
  // Chromium for its startup. This is called before any fuzz test case
  // is actually run, so unfortunately you can't generate these through
  // fuzzing. In addition, the browser test framework itself does all
  // sorts of fiddling with the arguments (e.g. a user data dir).
  // It's generally OK to leave this at the default unless you specifically
  // need to enable a feature or similar.
  // Do not include the executable name in your return value - that's
  // prepended automatically.
  virtual base::CommandLine::StringVector GetChromiumCommandLineArguments();

 protected:
  // Callback to actually do your fuzzing. This is called from the UI thread,
  // so you should take care not to block the thread too long. If you need
  // to run your fuzz case across multiple threads, consider a nested RunLoop.
  // Return 0 if the input is valid, -1 if it's invalid and should not be
  // evolved further by the fuzzing engine.
  virtual int Fuzz(const uint8_t* data, size_t size) = 0;

  // Should be called by subclasses from within Fuzz if they believe that
  // a fuzz case is going to take infinite time to run. This will arrange
  // to communicate this status to the fuzz engine as far as possible,
  // then for the whole process to exit, thus throwing away that fuzz case.
  // However, after calling this method, Fuzz should return -1 to indicate
  // invalid input.
  // The normal pattern for using this is, within Fuzz, to do this:
  // 1. Create a RunLoop but don't start it yet
  // 2. Start a OneShotTimer which calls this method then stops the RunLoop
  // 3. Start an async task which will run the test case, cancel the timer,
  //    and then stop the run loop
  // 4. Start the RunLoop.
  // If the test case turns out not actually to be infinite, step 3 could
  // cause a UaF, so this pattern can probably be improved in future.
  void DeclareInfiniteLoop() { exit_after_fuzz_case_ = true; }

 private:
  int DoFuzz(const uint8_t* data, size_t size);

  // Changes run loop timeout behavior to silently continue running the
  // test/fuzzer instead of failing. Timed out run loops will stop running,
  // but the rest of the test will continue executing.
  void KeepRunningOnTimeout();

  // Changes run loop timeouts behaviour to call `DeclareInfiniteLoop()`.
  void DeclareInfiniteLoopOnTimeout();

 private:
  std::vector<std::string> libfuzzer_command_line_;
  bool exit_after_fuzz_case_ = false;
  InProcessFuzzerOptions options_;
};

class InProcessFuzzerFactoryBase {
 public:
  virtual std::unique_ptr<InProcessFuzzer> CreateInProcessFuzzer() = 0;
};

extern InProcessFuzzerFactoryBase* g_in_process_fuzzer_factory;

// Class used to register a single in-process fuzzer in each executable.
template <typename T>
class InProcessFuzzerFactory : public InProcessFuzzerFactoryBase {
 public:
  InProcessFuzzerFactory() { g_in_process_fuzzer_factory = this; }
  std::unique_ptr<InProcessFuzzer> CreateInProcessFuzzer() override {
    return std::make_unique<T>();
  }
};

#define REGISTER_IN_PROCESS_FUZZER(fuzzer_class) \
  InProcessFuzzerFactory<fuzzer_class> fuzzer_instance;

#endif  // CHROME_TEST_FUZZING_IN_PROCESS_FUZZER_H_
