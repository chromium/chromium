// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/cfi_buildflags.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

// Helper class to know how far in the loading process the current WebContents
// has come. It will call the callback after DOMContentLoaded is called for
// the main frame.
class WebContentsMainFrameHelper : public content::WebContentsObserver {
 public:
  WebContentsMainFrameHelper(content::WebContents* web_contents,
                             base::OnceClosure callback)
      : WebContentsObserver(web_contents), callback_(std::move(callback)) {}

  void DOMContentLoaded(content::RenderFrameHost* render_frame_host) override {
    if (!render_frame_host->GetParent() && callback_)
      std::move(callback_).Run();
  }

 private:
  base::OnceClosure callback_;
};

}  // namespace

namespace dom_distiller {

const char* kExternalTestResourcesPath =
    "third_party/dom_distiller_js/dist/test/data";
// TODO(crbug.com/40590818): Remove filter once image construction happens
// synchronously and asserts do not flake anymore when exposed to different
// garbage collection heuristics.
const char* kTestFilePath =
    "/war/test.html?console_log=0&filter="
    "-*.testImageExtractorWithAttributesCSSHeightCM"
    ":*.testImageExtractorWithHeightCSS"
    ":*.testImageExtractorWithOneAttribute"
    ":*.testImageExtractorWithSettingDimension";
const char* kRunJsTestsJs =
    "(function() {return org.chromium.distiller.JsTestEntry.run();})();";

class DomDistillerJsTest : public content::ContentBrowserTest {
 public:
  DomDistillerJsTest() = default;

  // content::ContentBrowserTest:
  void SetUpOnMainThread() override {
    AddComponentsResources();
    SetUpTestServer();
    content::ContentBrowserTest::SetUpOnMainThread();
  }

  void OnJsTestExecutionDone(base::Value value) {
    result_ = std::move(value);
    std::move(js_test_execution_done_callback_).Run();
  }

 protected:
  base::OnceClosure js_test_execution_done_callback_;
  base::Value result_;

 private:
  void AddComponentsResources() {
    base::FilePath pak_file;
    base::FilePath pak_dir;
#if BUILDFLAG(IS_ANDROID)
    CHECK(base::PathService::Get(base::DIR_ANDROID_APP_DATA, &pak_dir));
    pak_dir = pak_dir.Append(FILE_PATH_LITERAL("paks"));
#elif BUILDFLAG(IS_MAC)
    base::PathService::Get(base::DIR_MODULE, &pak_dir);
#else
    base::PathService::Get(base::DIR_ASSETS, &pak_dir);
#endif  // BUILDFLAG(IS_ANDROID)
    pak_file =
        pak_dir.Append(FILE_PATH_LITERAL("components_tests_resources.pak"));
    ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
        pak_file, ui::kScaleFactorNone);
  }

  void SetUpTestServer() {
    base::FilePath path;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &path);
    path = path.AppendASCII(kExternalTestResourcesPath);
    embedded_test_server()->ServeFilesFromDirectory(path);
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

// Disabled on MSan as well as Android and Linux CFI bots.
// https://crbug.com/845180
// https://crbug.com/1434395
// Then disabled more generally on Android: https://crbug.com/979685
// TODO(jaebaek):  HTMLImageElement::LayoutBoxWidth() returns a value that has
// a small error from the real one (i.e., the real is 38, but it returns 37)
// and it results in the failure of
// EmbedExtractorTest.testImageExtractorWithAttributesCSSHeightCM (See
// crrev.com/c/916021). We must solve this precision issue.
#if defined(MEMORY_SANITIZER) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID) || \
    BUILDFLAG(IS_FUCHSIA) ||                                                   \
    ((BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) &&                        \
     (BUILDFLAG(CFI_CAST_CHECK) || BUILDFLAG(CFI_ICALL_CHECK) ||               \
      BUILDFLAG(CFI_ENFORCEMENT_DIAGNOSTIC) ||                                 \
      BUILDFLAG(CFI_ENFORCEMENT_TRAP)))
#define MAYBE_RunJsTests DISABLED_RunJsTests
#else
#define MAYBE_RunJsTests RunJsTests
#endif
IN_PROC_BROWSER_TEST_F(DomDistillerJsTest, MAYBE_RunJsTests) {
  // Load the test file in content shell and wait until it has fully loaded.
  content::WebContents* web_contents = shell()->web_contents();
  base::RunLoop url_loaded_runner;
  WebContentsMainFrameHelper main_frame_loaded(web_contents,
                                               url_loaded_runner.QuitClosure());
  web_contents->GetController().LoadURL(
      embedded_test_server()->GetURL(kTestFilePath),
      content::Referrer(),
      ui::PAGE_TRANSITION_TYPED,
      std::string());
  url_loaded_runner.Run();

  // Execute the JS to run the tests, and wait until it has finished.
  base::RunLoop run_loop;
  js_test_execution_done_callback_ = run_loop.QuitClosure();
  // Add timeout in case JS Test execution fails. It is safe to call the
  // QuitClosure multiple times.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), TestTimeouts::action_max_timeout());
  web_contents->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
      base::UTF8ToUTF16(kRunJsTestsJs),
      base::BindOnce(&DomDistillerJsTest::OnJsTestExecutionDone,
                     base::Unretained(this)),
      content::ISOLATED_WORLD_ID_GLOBAL);
  run_loop.Run();

  // Convert to dictionary and parse the results.
  ASSERT_TRUE(result_.is_dict()) << "Result is not a dictionary: " << result_;

  const base::Value::Dict& dict = result_.GetDict();
  std::optional<bool> success = dict.FindBool("success");
  ASSERT_TRUE(success.has_value());
  std::optional<int> num_tests = dict.FindInt("numTests");
  ASSERT_TRUE(num_tests.has_value());
  std::optional<int> failed = dict.FindInt("failed");
  ASSERT_TRUE(failed.has_value());
  std::optional<int> skipped = dict.FindInt("skipped");
  ASSERT_TRUE(skipped.has_value());

  VLOG(0) << "Ran " << num_tests.value()
          << " tests. failed = " << failed.value()
          << " skipped = " << skipped.value();
  // Ensure that running the tests succeeded.
  EXPECT_TRUE(success.value());

  // Only print the log if there was an error.
  if (!success.value()) {
    const std::string* console_log = dict.FindString("log");
    ASSERT_TRUE(console_log);
    VLOG(0) << "Console log:\n" << *console_log;
    VLOG(0) << "\n\n"
        "More info at third_party/dom_distiller_js/README.chromium.\n"
        "To disable tests, modify the filter parameter in |kTestFilePath|,\n"
        "in gtest_filter syntax.\n\n";
  }
}

}  // namespace dom_distiller
