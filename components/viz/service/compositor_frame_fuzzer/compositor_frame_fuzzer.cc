// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/task/single_thread_task_executor.h"
#include "components/viz/service/compositor_frame_fuzzer/compositor_frame_fuzzer_util.h"
#include "components/viz/service/compositor_frame_fuzzer/fuzzer_browser_process.h"
#include "mojo/core/embedder/embedder.h"
#include "testing/libfuzzer/libfuzzer_exports.h"
#include "testing/libfuzzer/proto/lpm_interface.h"

#include "components/viz/service/compositor_frame_fuzzer/compositor_frame_fuzzer.pb.h"

namespace {

struct Env {
 public:
  Env() {
    mojo::core::Init();

    // Run fuzzer with the flag --dump-to-png[=dir-name] to dump the browser
    // display into PNG files for debugging purposes.
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    auto png_dir_path =
        command_line->HasSwitch("dump-to-png")
            ? base::Optional<base::FilePath>(
                  command_line->GetSwitchValuePath("dump-to-png"))
            : base::nullopt;

    // Re-initialize logging in order to pick up any command-line parameters
    // (such as --v=1 to enable verbose logging).
    logging::LoggingSettings settings;
    settings.logging_dest =
        logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
    logging::InitLogging(settings);

    browser_process = std::make_unique<viz::FuzzerBrowserProcess>(png_dir_path);
  }
  ~Env() = default;

  std::unique_ptr<viz::FuzzerBrowserProcess> browser_process;

 private:
  base::SingleThreadTaskExecutor single_thread_task_executor_;

  // Instantiation needed to make histogram macros in the SubmitCompositorFrame
  // flow work when verbosity is on.
  base::AtExitManager exit_manager_;
};

}  // namespace

extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv) {
  base::CommandLine::Init(*argc, *argv);
  return 0;
}

DEFINE_BINARY_PROTO_FUZZER(const viz::proto::RenderPass& render_pass_spec) {
  static base::NoDestructor<Env> env;

  viz::FuzzedData fuzzed_frame =
      viz::BuildFuzzedCompositorFrame(render_pass_spec);

  env->browser_process->EmbedFuzzedCompositorFrame(
      std::move(fuzzed_frame.frame), std::move(fuzzed_frame.allocated_bitmaps));
}
