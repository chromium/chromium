// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/fuzzing/in_process_fuzzer.h"

#include <vector>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/functional/callback_forward.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/test_timeouts.h"
#include "chrome/renderer/chrome_content_renderer_client.h"
#include "chrome/test/base/chrome_test_launcher.h"
#include "chrome/test/fuzzing/in_process_fuzzer_buildflags.h"
#include "content/public/app/content_main.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/test/test_launcher.h"
#include "third_party/blink/public/web/web_testing_support.h"

#if BUILDFLAG(IS_WIN)
#include "base/strings/sys_string_conversions.h"
#endif  // BUILDFLAG(IS_WIN)

// This is provided within libfuzzer, and documented, but is not its headers.
extern "C" int LLVMFuzzerRunDriver(int* argc,
                                   char*** argv,
                                   int (*UserCb)(const uint8_t* Data,
                                                 size_t Size));

namespace {

std::string_view RunLoopTimeoutBehaviorToString(
    RunLoopTimeoutBehavior behavior) {
  switch (behavior) {
    case RunLoopTimeoutBehavior::kDefault:
      return "kDefault";
    case RunLoopTimeoutBehavior::kContinue:
      return "kContinue";
    case RunLoopTimeoutBehavior::kDeclareInfiniteLoop:
      return "kDeclareInfiniteLoop";
  }
  return "kUnknown";
}

void LogRunLoopTimeoutCallback(RunLoopTimeoutBehavior behavior) {
  LOG(INFO) << "Custom RunLoop timeout callback triggered ("
            << RunLoopTimeoutBehaviorToString(behavior) << ").";
}

}  // namespace

InProcessFuzzerFactoryBase* g_in_process_fuzzer_factory;

InProcessFuzzer::InProcessFuzzer(InProcessFuzzerOptions options)
    : options_(options) {}

InProcessFuzzer::~InProcessFuzzer() = default;

base::CommandLine::StringVector
InProcessFuzzer::GetChromiumCommandLineArguments() {
  base::CommandLine::StringVector empty;
  return empty;
}

void InProcessFuzzer::SetUp() {
  // Overrides the default 60s run loop timeout set by `BrowserTestBase`. See
  // https://source.chromium.org/chromium/chromium/src/+/main:content/public/test/browser_test_base.cc?q=ScopedRunLoopTimeout.
  // All of the fuzzing engines that we use are having timeouts features, and
  // this timeout can vary depending on the number of tested testcases. We must
  // let the engines handle timeouts, and set the maximum here.
  base::test::ScopedRunLoopTimeout scoped_timeout(FROM_HERE,
                                                  base::TimeDelta::Max());

  switch (options_.run_loop_timeout_behavior) {
    case RunLoopTimeoutBehavior::kContinue:
      KeepRunningOnTimeout();
      break;
    case RunLoopTimeoutBehavior::kDeclareInfiniteLoop:
      DeclareInfiniteLoopOnTimeout();
      break;
    case RunLoopTimeoutBehavior::kDefault:
      break;
  }

  // Note that browser tests are being launched by the `SetUp` method.
  InProcessBrowserTest::SetUp();
}

void InProcessFuzzer::KeepRunningOnTimeout() {
  base::test::ScopedRunLoopTimeout::SetTimeoutCallbackForTesting(
      std::make_unique<base::test::ScopedRunLoopTimeout::TimeoutCallback>(
          base::IgnoreArgs<const base::Location&,
                           base::RepeatingCallback<std::string()>,
                           const base::Location&>(base::BindRepeating(
              &LogRunLoopTimeoutCallback, RunLoopTimeoutBehavior::kContinue))));
}

void InProcessFuzzer::DeclareInfiniteLoopOnTimeout() {
  base::test::ScopedRunLoopTimeout::SetTimeoutCallbackForTesting(
      std::make_unique<base::test::ScopedRunLoopTimeout::TimeoutCallback>(
          base::IgnoreArgs<const base::Location&,
                           base::RepeatingCallback<std::string()>,
                           const base::Location&>(
              base::BindRepeating(&InProcessFuzzer::DeclareInfiniteLoop,
                                  base::Unretained(this)))
              .Then(base::BindRepeating(
                  &LogRunLoopTimeoutCallback,
                  RunLoopTimeoutBehavior::kDeclareInfiniteLoop))));
}

void InProcessFuzzer::Run(
    const std::vector<std::string>& libfuzzer_command_line) {
  libfuzzer_command_line_ = libfuzzer_command_line;
  SetUp();
  TearDown();
}

void InProcessFuzzer::SetUpOnMainThread() {
  InProcessBrowserTest::SetUpOnMainThread();

  // All of the engines that are being used to run those fuzzers are handling
  // process interruption. In case we let Chrome handle those signals itself,
  // we end up exiting the fuzzing process, and the engine records the last
  // run as a crash since it cannot not determine the reason why the process
  // terminated.
#if BUILDFLAG(IS_POSIX)
  signal(SIGTERM, SIG_DFL);
  signal(SIGINT, SIG_DFL);
#if defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
  // In case we're being built with a memory tool (asan, msan...), we should
  // let it handle this signal so that we get better reporting.
  // As of now, since both in-process stack traces and the crashpad handler are
  // being disabled, this is the only signal that we need to reset since it's
  // being set in
  // https://source.chromium.org/chromium/chromium/src/+/main:content/public/test/browser_test_base.cc?q=SignalHandler
  signal(SIGSEGV, SIG_DFL);
#endif  // BUILDFLAG(MEMORY_TOOL_REPLACES_ALLOCATOR)
#endif  // BUILDFLAG(IS_POSIX)
}

InProcessFuzzer* g_test;

// The following three classes are only meant to inject `internals` into JS.
// This object is needed by our IPC based fuzzers, and could also be needed by
// other in the future.
class InternalsObjectFrameInjector : public content::RenderFrameObserver {
 public:
  explicit InternalsObjectFrameInjector(content::RenderFrame* render_frame)
      : content::RenderFrameObserver(render_frame) {}
  void DidClearWindowObject() override {
    blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
    blink::WebTestingSupport::InjectInternalsObject(frame);
  }
  void OnDestruct() override { delete this; }
};

class InternalsObjectRendererInjector : public ChromeContentRendererClient {
 public:
  void RenderFrameCreated(content::RenderFrame* render_frame) override {
    new InternalsObjectFrameInjector(render_frame);
  }
};

class FuzzerChromeMainDelegate : public ChromeTestChromeMainDelegate {
 public:
  FuzzerChromeMainDelegate() = default;
  content::ContentRendererClient* CreateContentRendererClient() override {
    return new InternalsObjectRendererInjector();
  }
};

class FuzzerTestLauncherDelegate : public content::TestLauncherDelegate {
 public:
  FuzzerTestLauncherDelegate(std::unique_ptr<InProcessFuzzer>&& fuzzer,
                             std::vector<std::string>&& libfuzzer_arguments)
      : fuzzer_(std::move(fuzzer)),
        libfuzzer_arguments_(std::move(libfuzzer_arguments)) {
    content_main_delegate_ = std::make_unique<FuzzerChromeMainDelegate>();
  }

  int RunTestSuite(int argc, char** argv) override {
    fuzzer_->Run(libfuzzer_arguments_);
    return 0;
  }
#if !BUILDFLAG(IS_ANDROID)
  // Android browser tests set the ContentMainDelegate itself for the test
  // harness to use, and do not go through ContentMain() in TestLauncher.
  content::ContentMainDelegate* CreateContentMainDelegate() override {
    return &*content_main_delegate_;
  }
#endif

 private:
  std::unique_ptr<InProcessFuzzer> fuzzer_;
  std::unique_ptr<content::ContentMainDelegate> content_main_delegate_;
  std::vector<std::string> libfuzzer_arguments_;
};

int fuzz_callback(const uint8_t* data, size_t size) {
  return g_test->DoFuzz(data, size);
}

void InProcessFuzzer::RunTestOnMainThread() {
  std::vector<char*> argv;
  for (const auto& arg : libfuzzer_command_line_) {
    argv.push_back((char*)arg.data());
  }
  argv.push_back(nullptr);
  int argc = argv.size() - 1;
  char** argv2 = argv.data();
  g_test = this;
  base::IgnoreResult(LLVMFuzzerRunDriver(&argc, &argv2, fuzz_callback));
  if (exit_after_fuzz_case_) {
    LOG(INFO) << "Early exit requested - exiting after LLVMFuzzerRunDriver.";
    exit(0);
  }
  g_test = nullptr;
}

int InProcessFuzzer::DoFuzz(const uint8_t* data, size_t size) {
  // We actually exit before running the *next* fuzz case to give an opportunity
  // to return the return value to the fuzzing engine.
  if (exit_after_fuzz_case_) {
    LOG(INFO) << "Early exit requested - exiting after fuzz case.";
    exit(0);
  }
  std::optional<base::test::ScopedRunLoopTimeout> scoped_timeout;
  if (options_.run_loop_timeout) {
    scoped_timeout.emplace(FROM_HERE, *options_.run_loop_timeout);
  }
  return Fuzz(data, size);
}

// Main function for running in process fuzz tests.
// This aims to replicate //chrome browser tests as much as possible; we want
// the whole browser environment to be available for this sort of test in as
// realistic a fashion as possible.
int main(int argc, char** argv) {
  base::AtExitManager atexit_manager;
  base::CommandLine::Init(argc, argv);

  std::unique_ptr<InProcessFuzzer> fuzzer =
      g_in_process_fuzzer_factory->CreateInProcessFuzzer();

  // Oh dear, you've got to the part of the code relating to command lines.
  // I'm sorry.
  // Here are our constraints:
  // * Both libfuzzer/centipede and Chromium expect a full command line
  // * We set the format of neither command line
  // * Chromium will launch other Chromium processes, giving them a command
  // line.
  // * The centipede runner will launch our fuzzer, giving it a command line.
  // So, at this point, we have to figure out heuristics for what's up.
  // Are we the original fuzzer process, in which case we pass the CLI to
  // libfuzzer/centipede, and ask for a suitable Chromium command line from
  // our fuzz test? Or, are we a child Chromium process which has been
  // launched from a previous Chromium process? Well, dear reader, there are
  // no telltail arguments guaranteed to be on either, so we're going to
  // use a heuristic. If the first argument starts with --, we're assuming
  // we're a Chromium child.

  bool we_are_probably_a_chromium_child_process = false;
  if (base::CommandLine::ForCurrentProcess()->argv().size() > 1) {
    if (base::StartsWith(base::CommandLine::ForCurrentProcess()->argv()[1],
                         FILE_PATH_LITERAL("--"))) {
      we_are_probably_a_chromium_child_process = true;
    }
  }
  std::vector<std::string> libfuzzer_arguments;
  if (we_are_probably_a_chromium_child_process) {
    // If we're a Chromium child, we don't alter the command-line,
    // and in fact the libfuzzer code will never run, so we don't need to
    // pass any arguments through to libfuzzer.
  } else {
#if BUILDFLAG(IS_WIN)
    // Convert std::wstring (Windows command lines) to std::string
    // (as needed by libfuzzer).
    auto wide_argv = base::CommandLine::ForCurrentProcess()->argv();
    for (auto arg : wide_argv) {
      libfuzzer_arguments.push_back(base::SysWideToUTF8(arg));
    }
#else
    libfuzzer_arguments = base::CommandLine::ForCurrentProcess()->argv();
#endif  // BUILDFLAG(IS_WIN)
    base::CommandLine::StringType executable_name =
        base::CommandLine::ForCurrentProcess()->argv().at(0);
    base::CommandLine::StringVector chromium_arguments =
        fuzzer->GetChromiumCommandLineArguments();
    chromium_arguments.insert(chromium_arguments.begin(), executable_name);
    chromium_arguments.push_back(FILE_PATH_LITERAL("--single-process-tests"));
    chromium_arguments.push_back(FILE_PATH_LITERAL("--single-process"));
    chromium_arguments.push_back(FILE_PATH_LITERAL("--no-sandbox"));
    chromium_arguments.push_back(FILE_PATH_LITERAL("--no-zygote"));
    chromium_arguments.push_back(FILE_PATH_LITERAL("--disable-gpu"));
    chromium_arguments.push_back(
        FILE_PATH_LITERAL("--disable-crashpad-for-testing"));
#if defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
    // We disable in-process stack trace handling in case we're using memory
    // tools so that we get better reporting on what happened in case of
    // SIGSEGV.
    chromium_arguments.push_back(
        FILE_PATH_LITERAL("--disable-in-process-stack-traces"));
#endif
    base::CommandLine::ForCurrentProcess()->InitFromArgv(chromium_arguments);

    // Various bits of setup are done by base::TestSuite::Initialize.
    // As we're not a functional test suite, most of those things are not
    // necessary, but at least this is:
    TestTimeouts::Initialize();
  }

  FuzzerTestLauncherDelegate* fuzzer_launcher_delegate =
      new FuzzerTestLauncherDelegate(std::move(fuzzer),
                                     std::move(libfuzzer_arguments));
  return LaunchChromeTests(1, fuzzer_launcher_delegate, argc, argv);
}
