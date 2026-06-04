// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <csignal>
#include <optional>
#include <string_view>

#include "base/base_paths.h"
#include "base/path_service.h"
#include "base/strings/escape.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/fuzzing/in_process_fuzzer.h"
#include "chrome/test/fuzzing/in_process_fuzzer_buildflags.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

// This is an example use of the InProcessFuzzer framework.
// It runs arbitrary JS within the context of an existing
// loaded page, which is much quicker than loading a new
// page each time. It has no awareness of JS syntax so
// it's unlikely to be an effective fuzzer; future
// developments may feed it a useful corpus or dictionary
// or add a mutator. This is a first step in that direction.

class JsInProcessFuzzer : public InProcessFuzzer {
 public:
  JsInProcessFuzzer();
  void SetUpOnMainThread() override;
  base::CommandLine::StringVector GetChromiumCommandLineArguments() override;

  int Fuzz(const uint8_t* data, size_t size) override;
};

REGISTER_IN_PROCESS_FUZZER(JsInProcessFuzzer)

namespace {

// We have a timeout to avoid JavaScript infinite loops hanging the
// fuzzer. Empirically, valid JS cases complete locally well within 2
// seconds so allow 8 seconds to account for slowness on fuzzing
// infrastructure.

#if BUILDFLAG(IS_FUZZILLI)
// Fuzzilli handles timeouts by itself so that it detects when there are
// infinite loops.
constexpr std::optional<base::TimeDelta> kJsExecutionTimeout;
constexpr RunLoopTimeoutBehavior kJsRunLoopTimeoutBehavior =
    RunLoopTimeoutBehavior::kDefault;
#else
constexpr std::optional<base::TimeDelta> kJsExecutionTimeout = base::Seconds(8);
constexpr RunLoopTimeoutBehavior kJsRunLoopTimeoutBehavior =
    RunLoopTimeoutBehavior::kDeclareInfiniteLoop;
#endif
constexpr char kMojoFuzzerHtml[] = R"(
<script src="gen/mojo/public/js/mojo_bindings_lite.js"></script>
<script src="gen/mojo/public/mojom/base/string16.mojom-lite.js"></script>
<script src="gen/url/mojom/scheme_host_port.mojom-lite.js"></script>
<script src="gen/url/mojom/url.mojom-lite.js"></script>
<script
 src="gen/third_party/blink/public/mojom/credentialmanagement/credential_manager.mojom-lite.js">
</script>
<script
 src="gen/third_party/blink/public/mojom/locks/lock_manager.mojom-lite.js">
</script>
)";

std::unique_ptr<net::test_server::HttpResponse> HandleMojoFuzzerRequest(
    const net::test_server::HttpRequest& request) {
  if (request.relative_url != "/mojo_fuzzer.html") {
    return nullptr;
  }
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_content_type("text/html");
  response->set_content(kMojoFuzzerHtml);
  return response;
}

}  // namespace

JsInProcessFuzzer::JsInProcessFuzzer()
    : InProcessFuzzer(InProcessFuzzerOptions{
          .run_loop_timeout_behavior = kJsRunLoopTimeoutBehavior,
          .run_loop_timeout = kJsExecutionTimeout,
      }) {}

void JsInProcessFuzzer::SetUpOnMainThread() {
  InProcessFuzzer::SetUpOnMainThread();

  base::FilePath build_dir;
  base::PathService::Get(base::DIR_EXE, &build_dir);

  embedded_https_test_server().RegisterRequestHandler(
      base::BindRepeating(&HandleMojoFuzzerRequest));

  embedded_https_test_server().ServeFilesFromDirectory(build_dir);
  ASSERT_TRUE(embedded_https_test_server().Start());

  GURL url = embedded_https_test_server().GetURL("/mojo_fuzzer.html");
  CHECK(ui_test_utils::NavigateToURL(browser(), url));
#if BUILDFLAG(IS_FUZZILLI)
  // Fuzzilli needs to see this. Unfortunately, we install a signal handler at
  // //content/public/test/browser_test_base.cc that exits when one of those
  // signals occur. Disabling it allows for Fuzzilli to see them.
  signal(SIGSEGV, SIG_DFL);
  signal(SIGTERM, SIG_DFL);
#endif
}

base::CommandLine::StringVector
JsInProcessFuzzer::GetChromiumCommandLineArguments() {
#if BUILDFLAG(IS_FUZZILLI)
  base::FilePath dir_parent("/tmp/fuzzilli_tmp");
  base::ScopedTempDir dir;
  bool created = dir.CreateUniqueTempDirUnderPath(dir_parent);
  CHECK(created) << "ScopedTempDir failed to create a unique directory under '"
                 << dir_parent.value() << "'";
  // Using Take() instead of GetPath() prevents the directory from being
  // deleted once `dir` goes out of scope.
  base::FilePath path_dir = dir.Take();
  std::string user_data_dir = "--user-data-dir=" + path_dir.value();
#endif
  return {
      FILE_PATH_LITERAL("--js-flags=--jit-fuzzing --allow-natives-syntax "
                        "--expose-gc --fuzzing --future --harmony"),
      FILE_PATH_LITERAL("--enable-blink-features=MojoJS,MojoJSTest"),
      FILE_PATH_LITERAL("--enable-experimental-web-platform-features"),
      // Disable this to avoid crashing the testing framework when invalid Mojo
      // messages are received which happens a lot for some interfaces. We're
      // trading off worse runtime accuracy for better fuzzer performance.
      FILE_PATH_LITERAL("--disable-kill-after-bad-ipc"),
#if BUILDFLAG(IS_FUZZILLI)
      // This was caused by some issues with disks filling up fast, because
      // Fuzzilli restarts the binary very frequently.
      user_data_dir,
#endif
  };
}

int JsInProcessFuzzer::Fuzz(const uint8_t* data, size_t size) {
  std::string_view js_str(reinterpret_cast<const char*>(data), size);
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* rfh = contents->GetPrimaryMainFrame();
  // Execute JS within an existing page - that's the essence of this
  // fuzzer and why it's much quicker than html_in_process_fuzzer
  // or page_load_in_process_fuzzer.
  // We use a synchronous function because we set the InProcessFuzzer's
  // kDeclareInfiniteLoop behaviour in case of infinite loops.
  // We invoke ExecJs with default options, which turns on user gestures in JS
  // so that this can in theory explore APIs which are gated behind that
  // restriction (subject to future developments with dictionaries, corpora,
  // etc.)
  testing::AssertionResult res = content::ExecJs(rfh, js_str);
  if (js_str.contains("EXPERIMENTAL_lock_manager_crash")) {
    raise(SIGSEGV);
  }

#if BUILDFLAG(IS_FUZZILLI)
  // Fuzzilli needs to know when an exception was uncaught.
  if (!res) {
    return -1;
  }
#endif
  return 0;
}
