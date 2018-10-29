// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/cfi_buildflags.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/dom_distiller/content/browser/web_contents_main_frame_observer.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

// Helper class to know how far in the loading process the current WebContents
// has come. It will call the callback after DocumentLoadedInFrame is called for
// the main frame.
class WebContentsMainFrameHelper : public content::WebContentsObserver {
 public:
  WebContentsMainFrameHelper(content::WebContents* web_contents,
                             const base::Closure& callback)
      : WebContentsObserver(web_contents), callback_(callback) {}

  void DocumentLoadedInFrame(
      content::RenderFrameHost* render_frame_host) override {
    if (!render_frame_host->GetParent())
      callback_.Run();
  }

 private:
  base::Closure callback_;
};

}  // namespace

namespace dom_distiller {

const char* kExternalTestResourcesPath =
    "third_party/dom_distiller_js/dist/test/data";
// TODO(877461): Remove filter once image construction happens synchronously and
// asserts do not flake anymore when exposed to different garbage collection
// heuristics.
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
  DomDistillerJsTest() : result_(nullptr) {}

  // content::ContentBrowserTest:
  void SetUpOnMainThread() override {
    AddComponentsResources();
    SetUpTestServer();
    content::ContentBrowserTest::SetUpOnMainThread();
  }

  void OnJsTestExecutionDone(const base::Value* value) {
    result_ = value->DeepCopy();
    js_test_execution_done_callback_.Run();
  }

 protected:
  base::Closure js_test_execution_done_callback_;
  const base::Value* result_;

 private:
  void AddComponentsResources() {
    base::FilePath pak_file;
    base::FilePath pak_dir;
#if defined(OS_ANDROID)
    CHECK(base::PathService::Get(base::DIR_ANDROID_APP_DATA, &pak_dir));
    pak_dir = pak_dir.Append(FILE_PATH_LITERAL("paks"));
#else
    base::PathService::Get(base::DIR_MODULE, &pak_dir);
#endif  // OS_ANDROID
    pak_file =
        pak_dir.Append(FILE_PATH_LITERAL("components_tests_resources.pak"));
    ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
        pak_file, ui::SCALE_FACTOR_NONE);
  }

  void SetUpTestServer() {
    base::FilePath path;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &path);
    path = path.AppendASCII(kExternalTestResourcesPath);
    embedded_test_server()->ServeFilesFromDirectory(path);
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

// Disabled on MSan and Android CFI bots.
// https://crbug.com/845180
#if defined(MEMORY_SANITIZER) || defined(OS_WIN) ||              \
    (defined(OS_ANDROID) &&                                      \
     (BUILDFLAG(CFI_CAST_CHECK) || BUILDFLAG(CFI_ICALL_CHECK) || \
      BUILDFLAG(CFI_ENFORCEMENT_DIAGNOSTIC) ||                   \
      BUILDFLAG(CFI_ENFORCEMENT_TRAP)))
#define MAYBE_RunJsTests DISABLED_RunJsTests
#else
#define MAYBE_RunJsTests RunJsTests
#endif
IN_PROC_BROWSER_TEST_F(DomDistillerJsTest, MAYBE_RunJsTests) {
  // TODO(jaebaek): Revisit this code when the --use-zoom-for-dsf feature on
  // Android is done. If we remove this code (i.e., enable --use-zoom-for-dsf),
  // HTMLImageElement::LayoutBoxWidth() returns a value that has a small error
  // from the real one (i.e., the real is 38, but it returns 37) and it results
  // in the failure of
  // EmbedExtractorTest.testImageExtractorWithAttributesCSSHeightCM (See
  // crrev.com/c/916021). We must solve this precision issue.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kEnableUseZoomForDSF, "false");

  // Load the test file in content shell and wait until it has fully loaded.
  content::WebContents* web_contents = shell()->web_contents();
  dom_distiller::WebContentsMainFrameObserver::CreateForWebContents(
      web_contents);
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
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), TestTimeouts::action_max_timeout());
  web_contents->GetMainFrame()->ExecuteJavaScriptForTests(
      base::UTF8ToUTF16(kRunJsTestsJs),
      base::Bind(&DomDistillerJsTest::OnJsTestExecutionDone,
                 base::Unretained(this)));
  run_loop.Run();

  // By now either the timeout has triggered, or there should be a result.
  ASSERT_TRUE(result_ != nullptr) << "No result found. Timeout?";

  // Convert to dictionary and parse the results.
  const base::DictionaryValue* dict;
  result_->GetAsDictionary(&dict);
  ASSERT_TRUE(result_->GetAsDictionary(&dict));

  ASSERT_TRUE(dict->HasKey("success"));
  bool success;
  ASSERT_TRUE(dict->GetBoolean("success", &success));

  ASSERT_TRUE(dict->HasKey("numTests"));
  int num_tests;
  ASSERT_TRUE(dict->GetInteger("numTests", &num_tests));

  ASSERT_TRUE(dict->HasKey("failed"));
  int failed;
  ASSERT_TRUE(dict->GetInteger("failed", &failed));

  ASSERT_TRUE(dict->HasKey("skipped"));
  int skipped;
  ASSERT_TRUE(dict->GetInteger("skipped", &skipped));

  VLOG(0) << "Ran " << num_tests << " tests. failed = " << failed
          << " skipped = " << skipped;
  // Ensure that running the tests succeeded.
  EXPECT_TRUE(success);

  // Only print the log if there was an error.
  if (!success) {
    ASSERT_TRUE(dict->HasKey("log"));
    std::string console_log;
    ASSERT_TRUE(dict->GetString("log", &console_log));
    VLOG(0) << "Console log:\n" << console_log;
    VLOG(0) << "\n\n"
        "More info at third_party/dom_distiller_js/README.chromium.\n"
        "To disable tests, modify the filter parameter in |kTestFilePath|,\n"
        "in gtest_filter syntax.\n\n";
  }
}

}  // namespace dom_distiller
