// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/devtools/cast_devtools_manager_delegate.h"

#include <memory>
#include <string>
#include <unordered_set>

#include "base/functional/bind.h"
#include "base/test/scoped_command_line.h"
#include "chromecast/base/version.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/mock_devtools_agent_host.h"
#include "content/public/test/test_content_client_initializer.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/test/gl_surface_test_support.h"

namespace chromecast {
namespace shell {

namespace {

class TestWorkerDevToolsAgentHost : public content::MockDevToolsAgentHost {
 public:
  TestWorkerDevToolsAgentHost(std::string parent_id,
                              std::string parent_frame_id)
      : parent_id_(std::move(parent_id)),
        parent_frame_id_(std::move(parent_frame_id)) {}

  std::string GetType() override {
    return content::DevToolsAgentHost::kTypeDedicatedWorker;
  }
  std::string GetParentId() override { return parent_id_; }
  std::string GetParentFrameId() override { return parent_frame_id_; }
  content::WebContents* GetWebContents() override { return nullptr; }

 private:
  ~TestWorkerDevToolsAgentHost() override = default;

  std::string parent_id_;
  std::string parent_frame_id_;
};

}  // namespace

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
      EXPECT_TRUE(enabled_web_contents.contains(target->GetWebContents()))
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

TEST_F(CastDevToolsManagerDelegateTest, AllowInspectingTarget) {
  EXPECT_FALSE(devtools_manager_delegate_->AllowInspectingTarget(nullptr));

  scoped_refptr<content::DevToolsAgentHost> page_host =
      content::DevToolsAgentHost::GetOrCreateFor(web_contents());
  scoped_refptr<content::DevToolsAgentHost> tab_host =
      content::DevToolsAgentHost::GetOrCreateForTab(web_contents());
  EXPECT_FALSE(
      devtools_manager_delegate_->AllowInspectingTarget(page_host.get()));
  EXPECT_FALSE(
      devtools_manager_delegate_->AllowInspectingTarget(tab_host.get()));

  devtools_manager_delegate_->EnableWebContentsForDebugging(web_contents());
  EXPECT_TRUE(
      devtools_manager_delegate_->AllowInspectingTarget(page_host.get()));
  EXPECT_TRUE(
      devtools_manager_delegate_->AllowInspectingTarget(tab_host.get()));

  // A second non-enabled WebContents must remain disallowed while the first
  // WebContents is enabled.
  std::unique_ptr<content::WebContents> other_web_contents =
      CreateTestWebContents();
  scoped_refptr<content::DevToolsAgentHost> other_page_host =
      content::DevToolsAgentHost::GetOrCreateFor(other_web_contents.get());
  EXPECT_FALSE(
      devtools_manager_delegate_->AllowInspectingTarget(other_page_host.get()));

  // Child worker targets inherit null GetWebContents() and should be allowed
  // iff their parent (via GetParentId() or GetParentFrameId()) belongs to an
  // enabled WebContents.
  auto enabled_worker_by_parent_id =
      base::MakeRefCounted<TestWorkerDevToolsAgentHost>(page_host->GetId(),
                                                        /*parent_frame_id=*/"");
  auto disabled_worker_by_parent_id =
      base::MakeRefCounted<TestWorkerDevToolsAgentHost>(
          other_page_host->GetId(), /*parent_frame_id=*/"");
  EXPECT_TRUE(devtools_manager_delegate_->AllowInspectingTarget(
      enabled_worker_by_parent_id.get()));
  EXPECT_FALSE(devtools_manager_delegate_->AllowInspectingTarget(
      disabled_worker_by_parent_id.get()));

  // Pre-existing workers created before a RenderFrameDevToolsAgentHost exists
  // have an empty GetParentId() and populate only GetParentFrameId().
  std::unique_ptr<content::WebContents> pre_existing_worker_wc =
      CreateTestWebContents();
  auto worker_by_frame_token =
      base::MakeRefCounted<TestWorkerDevToolsAgentHost>(
          /*parent_id=*/"", pre_existing_worker_wc->GetPrimaryMainFrame()
                                ->GetDevToolsFrameToken()
                                .ToString());
  EXPECT_FALSE(devtools_manager_delegate_->AllowInspectingTarget(
      worker_by_frame_token.get()));
  devtools_manager_delegate_->EnableWebContentsForDebugging(
      pre_existing_worker_wc.get());
  EXPECT_TRUE(devtools_manager_delegate_->AllowInspectingTarget(
      worker_by_frame_token.get()));
  devtools_manager_delegate_->DisableWebContentsForDebugging(
      pre_existing_worker_wc.get());

  // Destroying a WebContents detaches its DevToolsAgentHost (GetWebContents()
  // becomes nullptr), which must be rejected.
  other_web_contents.reset();
  EXPECT_EQ(other_page_host->GetWebContents(), nullptr);
  EXPECT_FALSE(
      devtools_manager_delegate_->AllowInspectingTarget(other_page_host.get()));

  devtools_manager_delegate_->DisableWebContentsForDebugging(web_contents());
  EXPECT_FALSE(
      devtools_manager_delegate_->AllowInspectingTarget(page_host.get()));
  EXPECT_FALSE(
      devtools_manager_delegate_->AllowInspectingTarget(tab_host.get()));
  EXPECT_FALSE(devtools_manager_delegate_->AllowInspectingTarget(
      enabled_worker_by_parent_id.get()));

  scoped_refptr<content::DevToolsAgentHost> browser_host =
      content::DevToolsAgentHost::CreateForBrowser(
          nullptr, content::DevToolsAgentHost::CreateServerSocketCallback());
  EXPECT_EQ(
      devtools_manager_delegate_->AllowInspectingTarget(browser_host.get()),
      static_cast<bool>(CAST_IS_DEBUG_BUILD()));

  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      switches::kRemoteDebuggingPort, "9222");
  EXPECT_TRUE(
      devtools_manager_delegate_->AllowInspectingTarget(browser_host.get()));
}

}  // namespace shell
}  // namespace chromecast
