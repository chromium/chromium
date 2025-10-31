// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/dom_distiller/content/browser/distiller_javascript_utils.h"
#include "components/dom_distiller/content/browser/test/test_util.h"
#include "components/dom_distiller/core/dom_distiller_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace dom_distiller {
namespace {

class DistilledPageJsTest : public content::ContentBrowserTest {
 protected:
  explicit DistilledPageJsTest()
      : content::ContentBrowserTest(), distilled_page_(nullptr) {
#if BUILDFLAG(IS_ANDROID)
    feature_list_.InitAndDisableFeature(kReaderModeDistillInApp);
#endif
  }
  ~DistilledPageJsTest() override = default;

  void SetUpOnMainThread() override {
    if (!DistillerJavaScriptWorldIdIsSet()) {
      SetDistillerJavaScriptWorldId(content::ISOLATED_WORLD_ID_CONTENT_END);
    }

    AddComponentsResources();
    distilled_page_ = SetUpTestServerWithDistilledPage(embedded_test_server());
  }

  void LoadAndExecuteTestScript(const std::string& file) {
    distilled_page_->AppendScriptFile(file);
    distilled_page_->Load(embedded_test_server(), shell()->web_contents());
    // First, run the test.
    EXPECT_TRUE(content::ExecJs(shell()->web_contents(), "mocha.run()"));
    // Then, wait for the test to complete.
    EXPECT_TRUE(
        content::ExecJs(shell()->web_contents(), "window.completePromise"));
  }

  std::unique_ptr<FakeDistilledPage> distilled_page_;
  base::test::ScopedFeatureList feature_list_;
};

// Pincher is only used on Android.
#if !BUILDFLAG(IS_ANDROID)
#define MAYBE_Pinch DISABLED_Pinch
#else
#define MAYBE_Pinch Pinch
#endif
IN_PROC_BROWSER_TEST_F(DistilledPageJsTest, MAYBE_Pinch) {
  LoadAndExecuteTestScript("pinch_tester.js");
}

// FontSizeSlider is only used on Desktop.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
#define MAYBE_FontSizeSlider DISABLED_FontSizeSlider
#else
#define MAYBE_FontSizeSlider FontSizeSlider
#endif
IN_PROC_BROWSER_TEST_F(DistilledPageJsTest, MAYBE_FontSizeSlider) {
  LoadAndExecuteTestScript("font_size_slider_tester.js");
}

IN_PROC_BROWSER_TEST_F(DistilledPageJsTest, SettingsDialogTest) {
  LoadAndExecuteTestScript("settings_dialog_tester.js");
}

IN_PROC_BROWSER_TEST_F(DistilledPageJsTest, AddClassesToYTIFramesTest) {
  LoadAndExecuteTestScript("add_classes_to_yt_iframes.js");
}

// Fails on Fuchsia ASAN.
#if BUILDFLAG(IS_FUCHSIA) && defined(ADDRESS_SANITIZER)
#define MAYBE_ImageClassifierTest DISABLED_ImageClassifierTest
#else
#define MAYBE_ImageClassifierTest ImageClassifierTest
#endif
IN_PROC_BROWSER_TEST_F(DistilledPageJsTest, MAYBE_ImageClassifierTest) {
  LoadAndExecuteTestScript("image_classifier_tester.js");
}

IN_PROC_BROWSER_TEST_F(DistilledPageJsTest, ListClassifierTest) {
  LoadAndExecuteTestScript("list_classifier_tester.js");
}

IN_PROC_BROWSER_TEST_F(DistilledPageJsTest, IdentifyEmptySVGsTest) {
  LoadAndExecuteTestScript("identify_empty_svgs_tester.js");
}

IN_PROC_BROWSER_TEST_F(DistilledPageJsTest, WrapTablesTest) {
  LoadAndExecuteTestScript("wrap_tables_tester.js");
}

}  // namespace
}  // namespace dom_distiller
