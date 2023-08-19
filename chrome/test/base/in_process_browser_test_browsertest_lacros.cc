// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/in_process_browser_test.h"

#include "base/strings/string_split.h"
#include "base/version.h"
#include "chrome/browser/speech/tts_crosapi_util.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(crbug.com/1368284): Remove this test when there are more use cases
// to verify the start ash chrome logic.
class StartUniqueAshBrowserTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    // Need to put this before starting lacros.
    StartUniqueAshChrome({"ClipboardHistoryRefresh", "Jelly"},
                         {"DisableLacrosTtsSupport"},
                         {"random-unused-example-cmdline"}, "Reason:Test");
    InProcessBrowserTest::SetUp();
  }

  void CheckExpectations() {
    EXPECT_TRUE(ash_process_.IsValid());

    // Verify that Lacros Tts support is enabled in Lacros. This feature is
    // disabled by default in Ash, and is enabled by the unique ash in this
    // test, and passed to Lacros via mojom::crosapi::BrowserInitParams.
    EXPECT_TRUE(tts_crosapi_util::ShouldEnableLacrosTtsSupport());
  }
};

IN_PROC_BROWSER_TEST_F(StartUniqueAshBrowserTest, StartAshChrome) {
  CheckExpectations();
}

class GetAshChromeVersionBrowserTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    ash_version_ = GetAshChromeVersion();
    InProcessBrowserTest::SetUp();
  }

 protected:
  base::Version ash_version_;
};

IN_PROC_BROWSER_TEST_F(GetAshChromeVersionBrowserTest, GetAshChromeVersion) {
  std::vector<std::string> versions =
      base::SplitString(ash_version_.GetString(), ".", base::KEEP_WHITESPACE,
                        base::SPLIT_WANT_ALL);
  ASSERT_EQ(versions.size(), 4U);
}
