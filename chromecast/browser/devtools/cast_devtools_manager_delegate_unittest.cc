// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/devtools/cast_devtools_manager_delegate.h"

#include <memory>
#include <unordered_set>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_content_client_initializer.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/test/gl_surface_test_support.h"

namespace chromecast {
namespace shell {

class CastDevToolsManagerDelegateTest
    : public content::RenderViewHostTestHarness {
 public:
  using WebContentsSet = std::unordered_set<content::WebContents*>;

  CastDevToolsManagerDelegateTest() {}

  CastDevToolsManagerDelegateTest(const CastDevToolsManagerDelegateTest&) =
      delete;
  CastDevToolsManagerDelegateTest& operator=(
      const CastDevToolsManagerDelegateTest&) = delete;

  ~CastDevToolsManagerDelegateTest() override {}

  void SetUp() override {
    gl::GLSurfaceTestSupport::InitializeOneOff();
    initializer_ = std::make_unique<content::TestContentClientInitializer>();
    content::RenderViewHostTestHarness::SetUp();
    devtools_manager_delegate_ =
        std::make_unique<CastDevToolsManagerDelegate>();
  }

  void TestDiscoveredTargets(const WebContentsSet& enabled_web_contents,
                             content::DevToolsAgentHost::List targets) {
    EXPECT_EQ(enabled_web_contents.size(), targets.size());

    for (const auto& target : targets) {
      EXPECT_TRUE(
          base::Contains(enabled_web_contents, target->GetWebContents()))
          << "Discovered target not found in enabled WebContents.";
    }
  }

 protected:
  std::unique_ptr<content::TestContentClientInitializer> initializer_;
  std::unique_ptr<CastDevToolsManagerDelegate> devtools_manager_delegate_;
};

TEST_F(CastDevToolsManagerDelegateTest, TestSingletonGetter) {
  EXPECT_EQ(devtools_manager_delegate_.get(),
            CastDevToolsManagerDelegate::GetInstance());
}

TEST_F(CastDevToolsManagerDelegateTest, DisabledWebContents) {
  TestDiscoveredTargets(WebContentsSet(),
                        devtools_manager_delegate_->RemoteDebuggingTargets(
                            content::DevToolsManagerDelegate::kFrame));
}

TEST_F(CastDevToolsManagerDelegateTest, EnabledWebContents) {
  devtools_manager_delegate_->EnableWebContentsForDebugging(web_contents());
  WebContentsSet enabled_web_contents({web_contents()});
  TestDiscoveredTargets(enabled_web_contents,
                        devtools_manager_delegate_->RemoteDebuggingTargets(
                            content::DevToolsManagerDelegate::kFrame));
}

}  // namespace shell
}  // namespace chromecast
