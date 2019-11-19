// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/media_app_ui/test/media_app_ui_browsertest.h"

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/threading/thread_restrictions.h"
#include "chromeos/components/media_app_ui/url_constants.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Path to the JS that is injected into the guest frame when it navigates.
constexpr char kTestScriptPath[] =
    "chromeos/components/media_app_ui/test/guest_query_receiver.js";

}  // namespace

class MediaAppUiBrowserTest::TestCodeInjector
    : public content::TestNavigationObserver {
 public:
  TestCodeInjector()
      : TestNavigationObserver(GURL(chromeos::kChromeUIMediaAppURL)) {
    WatchExistingWebContents();
    StartWatchingNewWebContents();
  }

  // TestNavigationObserver:
  void OnDidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (navigation_handle->GetURL().GetOrigin() !=
        GURL(chromeos::kChromeUIMediaAppGuestURL))
      return;

    base::FilePath source_root;
    ASSERT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &source_root));
    const auto script_path = source_root.AppendASCII(kTestScriptPath);

    base::ScopedAllowBlockingForTesting allow_blocking;
    std::string injected_content;
    ASSERT_TRUE(base::ReadFileToString(script_path, &injected_content));

    auto* render_frame_host = navigation_handle->GetRenderFrameHost();

    // Use ExecuteScript(), not ExecJs(), because of Content Security Policy
    // directive: "script-src chrome://resources 'self'"
    ASSERT_TRUE(content::ExecuteScript(render_frame_host, injected_content));
    TestNavigationObserver::OnDidFinishNavigation(navigation_handle);
  }
};

MediaAppUiBrowserTest::MediaAppUiBrowserTest() = default;

MediaAppUiBrowserTest::~MediaAppUiBrowserTest() = default;

void MediaAppUiBrowserTest::SetUpOnMainThread() {
  injector_ = std::make_unique<TestCodeInjector>();
  MojoWebUIBrowserTest::SetUpOnMainThread();
}
