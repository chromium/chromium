// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/web_applications/test/sandboxed_web_ui_test_base.h"

#include <vector>

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/test/base/js_test_api.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

class SandboxedWebUiAppTestBase::TestCodeInjector
    : public content::TestNavigationObserver {
 public:
  explicit TestCodeInjector(SandboxedWebUiAppTestBase* owner)
      : TestNavigationObserver(GURL(owner->host_url_)), owner_(owner) {
    WatchExistingWebContents();
    StartWatchingNewWebContents();
  }

  // TestNavigationObserver:
  void OnDidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (navigation_handle->GetURL().GetOrigin() != GURL(owner_->sandboxed_url_))
      return;

    auto* guest_frame = navigation_handle->GetRenderFrameHost();

    // Inject the JS Test API first (assertEquals, etc).
    std::vector<base::FilePath> scripts = JsTestApiConfig().default_libraries;
    scripts.insert(scripts.end(), owner_->scripts_.begin(),
                   owner_->scripts_.end());

    for (const auto& script : scripts) {
      // Use ExecuteScript(), not ExecJs(), because of Content Security Policy
      // directive: "script-src chrome://resources 'self'"
      ASSERT_TRUE(
          content::ExecuteScript(guest_frame, LoadJsTestLibrary(script)));
    }
    TestNavigationObserver::OnDidFinishNavigation(navigation_handle);
  }

 private:
  SandboxedWebUiAppTestBase* const owner_;
};

SandboxedWebUiAppTestBase::SandboxedWebUiAppTestBase(
    const std::string& host_url,
    const std::string& sandboxed_url,
    const std::vector<base::FilePath>& scripts)
    : host_url_(host_url), sandboxed_url_(sandboxed_url), scripts_(scripts) {}

SandboxedWebUiAppTestBase::~SandboxedWebUiAppTestBase() = default;

// static
std::string SandboxedWebUiAppTestBase::LoadJsTestLibrary(
    const base::FilePath& script_path) {
  base::FilePath source_root;
  EXPECT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &source_root));
  const auto full_script_path =
      script_path.IsAbsolute() ? script_path : source_root.Append(script_path);

  base::ScopedAllowBlockingForTesting allow_blocking;
  std::string injected_content;
  EXPECT_TRUE(base::ReadFileToString(full_script_path, &injected_content));
  return injected_content;
}

// static
content::RenderFrameHost* SandboxedWebUiAppTestBase::GetAppFrame(
    content::WebContents* web_ui) {
  // GetAllFrames does a breadth-first traversal. Assume the first subframe
  // is the app.
  std::vector<content::RenderFrameHost*> frames = web_ui->GetAllFrames();
  EXPECT_EQ(2u, frames.size());
  EXPECT_TRUE(frames[1]);
  return frames[1];
}

// static
content::EvalJsResult SandboxedWebUiAppTestBase::EvalJsInAppFrame(
    content::WebContents* web_ui,
    const std::string& script) {
  // Clients of this helper all run in the same isolated world.
  constexpr int kWorldId = 1;
  return EvalJs(GetAppFrame(web_ui), script,
                content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, kWorldId);
}

void SandboxedWebUiAppTestBase::SetUpOnMainThread() {
  injector_ = std::make_unique<TestCodeInjector>(this);
  MojoWebUIBrowserTest::SetUpOnMainThread();
}
