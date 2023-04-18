// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/functional/callback_forward.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "chrome/test/base/chrome_test_launcher.h"
#include "chrome/test/fuzzing/in_process_fuzz_test.h"
#include "chrome/test/fuzzing/in_process_fuzzing_buildflags.h"
#include "content/public/app/content_main.h"
#include "content/public/test/test_launcher.h"

#if BUILDFLAG(IS_WIN)
#include "base/strings/sys_string_conversions.h"
#endif  // BUILDFLAG(IS_WIN)

// This is provided within libfuzzer, and documented, but is not its headers.
extern "C" int LLVMFuzzerRunDriver(int* argc,
                                   char*** argv,
                                   int (*UserCb)(const uint8_t* Data,
                                                 size_t Size));

InProcessFuzzTestFactoryBase* g_in_process_fuzz_test_factory;

InProcessFuzzTest::InProcessFuzzTest() = default;

InProcessFuzzTest::~InProcessFuzzTest() = default;

base::CommandLine::StringVector
InProcessFuzzTest::GetChromiumCommandLineArguments() {
  base::CommandLine::StringVector empty;
  return empty;
}

void InProcessFuzzTest::Run(
    const std::vector<std::string>& libfuzzer_command_line) {
  libfuzzer_command_line_ = libfuzzer_command_line;
  SetUp();
  TearDown();
}

void InProcessFuzzTest::SetUpOnMainThread() {
  InProcessBrowserTest::SetUpOnMainThread();
}

InProcessFuzzTest* g_test;

class FuzzTestLauncherDelegate : public content::TestLauncherDelegate {
 public:
  FuzzTestLauncherDelegate(std::unique_ptr<InProcessFuzzTest>&& fuzz_test,
                           std::vector<std::string>&& libfuzzer_arguments)
      : fuzz_test_(std::move(fuzz_test)),
        libfuzzer_arguments_(std::move(libfuzzer_arguments)) {
    content_main_delegate_ =
        std::make_unique<ChromeTestChromeMainDelegate>(base::TimeTicks::Now());
  }

  int RunTestSuite(int argc, char** argv) override {
    fuzz_test_->Run(libfuzzer_arguments_);
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
  std::unique_ptr<InProcessFuzzTest> fuzz_test_;
  std::unique_ptr<content::ContentMainDelegate>
      content_main_delegate_;  // TODO remove unique_ptr
  std::vector<std::string> libfuzzer_arguments_;
};

int fuzz_callback(const uint8_t* data, size_t size) {
  return g_test->FuzzCallback(data, size);
}

int InProcessFuzzTest::FuzzCallback(const uint8_t* data, size_t size) {
  int result;
  base::RunLoop run_loop;

  base::RepeatingCallback<void()> run_fuzz_case_lambda =
      base::BindLambdaForTesting([&]() {
        result = Fuzz(data, size);
        run_loop.QuitClosure().Run();
      });
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_fuzz_case_lambda);
  run_loop.Run();
  return result;
}

void InProcessFuzzTest::FuzzCaseFinished(
    int* result_storage,
    const base::RepeatingClosure& quit_closure,
    int result) {
  *result_storage = result;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                              quit_closure);
}

void InProcessFuzzTest::RunTestOnMainThread() {
  std::vector<char*> argv;
  for (const auto& arg : libfuzzer_command_line_) {
    argv.push_back((char*)arg.data());
  }
  argv.push_back(nullptr);
  int argc = argv.size() - 1;
  char** argv2 = argv.data();
  g_test = this;
  base::IgnoreResult(LLVMFuzzerRunDriver(&argc, &argv2, fuzz_callback));
  g_test = nullptr;
}

// Main function for running in process fuzz tests.
// This aims to replicate //chrome browser tests as much as possible; we want
// the whole browser environment to be available for this sort of test in as
// realistic a fashion as possible.
int main(int argc, char** argv) {
  base::AtExitManager atexit_manager;
  base::CommandLine::Init(argc, argv);

  std::unique_ptr<InProcessFuzzTest> fuzzer =
      g_in_process_fuzz_test_factory->CreateInProcessFuzzer();

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
#if !BUILDFLAG(AVOID_SINGLE_PROCESS_MODE)
    // TODO(1038952): make libfuzzer compatible with single-process mode.
    // As it stands, single-process mode works with centipede (and is probably
    // desirable both in terms of fuzzing speed and correctly gathering
    // coverage information) but not yet with libfuzzer.
    chromium_arguments.push_back(FILE_PATH_LITERAL("--single-process"));
#endif  // BUILDFLAG(AVOID_SINGLE_PROCESS_MODE)
    chromium_arguments.push_back(FILE_PATH_LITERAL("--no-sandbox"));
    chromium_arguments.push_back(FILE_PATH_LITERAL("--no-zygote"));
    chromium_arguments.push_back(FILE_PATH_LITERAL("--disable-gpu"));
    base::CommandLine::ForCurrentProcess()->InitFromArgv(chromium_arguments);
  }

  FuzzTestLauncherDelegate* fuzz_test_launcher_delegate =
      new FuzzTestLauncherDelegate(std::move(fuzzer),
                                   std::move(libfuzzer_arguments));
  return content::LaunchTests(fuzz_test_launcher_delegate, 1, argc, argv);
}
