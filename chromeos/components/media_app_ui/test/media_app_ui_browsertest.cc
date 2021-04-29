// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/media_app_ui/test/media_app_ui_browsertest.h"

#include "base/files/file_path.h"
#include "chromeos/components/media_app_ui/media_app_guest_ui.h"
#include "chromeos/components/media_app_ui/media_app_ui.h"
#include "chromeos/components/media_app_ui/url_constants.h"
#include "chromeos/components/web_applications/test/sandboxed_web_ui_test_base.h"

namespace {

// File with utility functions for testing, defines `test_util`.
constexpr base::FilePath::CharType kWebUiTestUtil[] =
    FILE_PATH_LITERAL("chrome/test/data/webui/test_util.js");

// File that `kWebUiTestUtil` is dependent on, defines `cr`.
constexpr base::FilePath::CharType kCr[] =
    FILE_PATH_LITERAL("ui/webui/resources/js/cr.js");

// File containing the test utility library, shared with integration tests.
constexpr base::FilePath::CharType kTestLibraryPath[] = FILE_PATH_LITERAL(
    "chromeos/components/media_app_ui/test/dom_testing_helpers.js");

// Test cases that run in the guest context.
constexpr char kGuestTestCases[] = "media_app_guest_ui_browsertest.js";

// Path to test files loaded via the TestFileRequestFilter.
constexpr base::FilePath::CharType kTestFileLocation[] =
    FILE_PATH_LITERAL("chromeos/components/media_app_ui/test");

// Paths requested on the media-app origin that should be delivered by the test
// handler.
constexpr const char* kTestFiles[] = {
    kGuestTestCases,  "media_app_ui_browsertest.js", "driver.js",
    "test_worker.js", "guest_query_receiver.js",
};

}  // namespace

MediaAppUiBrowserTest::MediaAppUiBrowserTest()
    : SandboxedWebUiAppTestBase(
          chromeos::kChromeUIMediaAppURL,
          chromeos::kChromeUIMediaAppGuestURL,
          {base::FilePath(kTestLibraryPath), base::FilePath(kCr),
           base::FilePath(kWebUiTestUtil)},
          kGuestTestCases) {
  ConfigureDefaultTestRequestHandler(
      base::FilePath(kTestFileLocation),
      {std::begin(kTestFiles), std::end(kTestFiles)});
}

MediaAppUiBrowserTest::~MediaAppUiBrowserTest() = default;

// static
std::string MediaAppUiBrowserTest::AppJsTestLibrary() {
  return SandboxedWebUiAppTestBase::LoadJsTestLibrary(
      base::FilePath(kTestLibraryPath));
}
