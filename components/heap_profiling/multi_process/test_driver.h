// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HEAP_PROFILING_MULTI_PROCESS_TEST_DRIVER_H_
#define COMPONENTS_HEAP_PROFILING_MULTI_PROCESS_TEST_DRIVER_H_

#include <vector>

#include "base/memory/ref_counted_memory.h"
#include "base/process/process_handle.h"
#include "base/synchronization/waitable_event.h"
#include "base/values.h"
#include "components/services/heap_profiling/public/cpp/settings.h"
#include "components/services/heap_profiling/public/mojom/heap_profiling_client.mojom.h"
#include "partition_alloc/partition_alloc_for_testing.h"

namespace heap_profiling {

// This class runs tests for the Heap Profiling Service, a cross-platform,
// multi-process component.
//
// Chrome on Android does not support browser_tests. It does support
// content_browsertests, but those are not multi-process tests. On Android,
// processes have to be started via the Activity mechanism, and the test
// infrastructure does not support this.
//
// To avoid test-code duplication, all tests are pulled into this class.
// browser_tests will directly call this class. The android
// chrome_public_test_apk will invoke this class via a JNI shim. Since the
// latter is not running within the gtest framework, this class cannot use
// EXPECT* and ASSERT* macros. Instead, this class will return a bool indicating
// success of the entire test. On failure, errors will be output via LOG(ERROR).
// These will show up in the browser_tests output stream, and will be captured
// by logcat [the Android logging facility]. The latter is already the canonical
// mechanism for investigating test failures.
//
// Note: Outputting to stderr will not have the desired effect, since that is
// not captured by logcat.
class TestDriver {
 public:
  struct Options {
    // The profiling mode to test.
    Mode mode = Mode::kBrowser;

    // The stack profiling mode to test.
    mojom::StackMode stack_mode = mojom::StackMode::NATIVE_WITHOUT_THREAD_NAMES;

    // Whether the caller has already started profiling with the given mode.
    // When false, the test driver is responsible for starting profiling.
    bool profiling_already_started = false;
  };

  TestDriver();

  TestDriver(const TestDriver&) = delete;
  TestDriver& operator=(const TestDriver&) = delete;

  ~TestDriver();

  // If this is called on the content::BrowserThread::UI thread, then the
  // platform must support nested message loops. [This is currently not
  // supported on Android].
  //
  // Returns whether the test run was successful. Expectation/Assertion failures
  // will be printed via LOG(ERROR).
  bool RunTest(const Options& options);

 private:
  // Populates |has_started_| and then signals |wait_for_ui_thread_|.
  void GetHasStartedOnUIThread();

  // Populates |initialization_success_| with the result of
  // |RunInitializationOnUIThread|, and then signals |wait_for_ui_thread_|.
  void CheckOrStartProfilingOnUIThreadAndSignal();

  // If profiling is expected to already be started, confirm it.
  // Otherwise, start profiling with the given mode.
  // This method must only be called on platforms that supported nested run
  // loops on the UI thread.
  bool CheckOrStartProfilingOnUIThreadWithNestedRunLoops();

  // If profiling is expected to already be started, confirm it.
  // Otherwise, start profiling with the given mode.
  // This method must only be called on platforms that are running the
  // TestDriver from a non-UI thread, which allows for async signalling.
  bool CheckOrStartProfilingOnUIThreadWithAsyncSignalling();

  // Performs allocations. These are expected to be profiled.
  void MakeTestAllocations();

  // Collects a trace that contains a heap dump. The result is stored in
  // |serialized_trace_|.
  //
  // When |synchronous| is true, this method spins a nested message loop. When
  // |synchronous| is false, this method posts some tasks that will eventually
  // signal |wait_for_ui_thread_|.
  void CollectResults(bool synchronous);

  void TraceFinished(base::OnceClosure closure,
                     bool success,
                     std::string trace_json);

  bool ValidateBrowserAllocations(const base::Value::Dict& dump_json);
  bool ValidateRendererAllocations(const base::Value::Dict& dump_json);

  bool ShouldProfileBrowser();
  bool ShouldProfileRenderer();
  bool ShouldIncludeNativeThreadNames();
  bool HasNativeFrames();

  void WaitForProfilingToStartForBrowserUIThread();
  void WaitForProfilingToStartForAllRenderersUIThread();

  // Android does not support nested RunLoops. Instead, it signals
  // |wait_for_ui_thread_| when finished.
  void WaitForProfilingToStartForAllRenderersUIThreadAndSignal();
  void WaitForProfilingToStartForAllRenderersUIThreadCallback(
      std::vector<base::ProcessId> results);

  Options options_;

  // Allocations made by this class. Intentionally leaked, since deallocating
  // them would trigger a large number of IPCs, which is slow.
  std::vector<char*> leaks_;

  // Sum of size of all variadic allocations.
  size_t total_variadic_allocations_ = 0;

  // Use to make PA allocations, which should also be shimmed.
  partition_alloc::PartitionAllocatorAllowLeaksForTesting partition_allocator_;

  // Contains nothing until |CollectResults| has been called.
  std::string serialized_trace_;

  // Whether the test was invoked on the ui thread.
  bool running_on_ui_thread_ = true;

  // Whether the supervisor has started.
  bool has_started_ = false;

  // Whether an error has occurred.
  bool initialization_success_ = false;

  // When |true|, initialization will wait for the allocator shim to enable
  // before continuing.
  bool wait_for_profiling_to_start_ = false;

  base::WaitableEvent wait_for_ui_thread_;
};

}  // namespace heap_profiling

#endif  // COMPONENTS_HEAP_PROFILING_MULTI_PROCESS_TEST_DRIVER_H_
