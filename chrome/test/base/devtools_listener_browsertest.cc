// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/process/process_handle.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/devtools_listener.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_agent_host_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace coverage {

class DevToolsListenerBrowserTest : public content::DevToolsAgentHostObserver,
                                    public InProcessBrowserTest {
 public:
  DevToolsListenerBrowserTest() = default;

  DevToolsListenerBrowserTest(const DevToolsListenerBrowserTest&) = delete;
  DevToolsListenerBrowserTest& operator=(const DevToolsListenerBrowserTest&) =
      delete;

  void SetUpOnMainThread() override {
    process_id_ = base::GetUniqueIdForProcess().GetUnsafeValue();
    content::DevToolsAgentHost::AddObserver(this);
  }

  bool ShouldForceDevToolsAgentHostCreation() override { return true; }

  void DevToolsAgentHostCreated(content::DevToolsAgentHost* host) override {
    if (host->GetType() != content::DevToolsAgentHost::kTypePage &&
        host->GetType() != content::DevToolsAgentHost::kTypeFrame) {
      return;
    }
    CHECK(devtools_agent_.find(host) == devtools_agent_.end());
    devtools_agent_[host] =
        std::make_unique<DevToolsListener>(host, process_id_);
  }

  void DevToolsAgentHostAttached(content::DevToolsAgentHost* host) override {}

  void DevToolsAgentHostNavigated(content::DevToolsAgentHost* host) override {
    if (devtools_agent_.find(host) == devtools_agent_.end())
      return;
    devtools_agent_.find(host)->second->Navigated(host);
  }

  void DevToolsAgentHostDetached(content::DevToolsAgentHost* host) override {}

  void DevToolsAgentHostCrashed(content::DevToolsAgentHost* host,
                                base::TerminationStatus status) override {
    if (devtools_agent_.find(host) == devtools_agent_.end())
      return;
    LOG(FATAL) << "Host crashed: " << DevToolsListener::HostString(host);
  }

 protected:
  content::WebContents* NavigateToTestFile(const std::string& name) {
    base::FilePath test_data_dir;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
    embedded_test_server()->ServeFilesFromDirectory(test_data_dir);

    CHECK(embedded_test_server()->Start());
    GURL test_file_name_url = embedded_test_server()->GetURL('/' + name);
    CHECK(ui_test_utils::NavigateToURL(browser(), test_file_name_url));

    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void CollectCodeCoverage(const std::string& test_name) {
    base::ScopedAllowBlockingForTesting allow_blocking;

    CHECK(tmp_dir_.CreateUniqueTempDir());
    base::FilePath coverage_store =
        tmp_dir_.GetPath().AppendASCII("devtools_listener_browser_test");
    DevToolsListener::SetupCoverageStore(coverage_store);

    for (auto& agent : devtools_agent_) {
      auto* host = agent.first;
      EXPECT_TRUE(agent.second->HasCoverage(host));
      agent.second->GetCoverage(host, coverage_store, test_name);
      agent.second->Detach(host);
    }

    ASSERT_FALSE(base::IsDirectoryEmpty(coverage_store));
  }

 private:
  base::ScopedTempDir tmp_dir_;

  using DevToolsAgentMap =  // agent hosts: have a unique devtools listener
      std::map<content::DevToolsAgentHost*, std::unique_ptr<DevToolsListener>>;

  DevToolsAgentMap devtools_agent_;
  uint32_t process_id_ = 0;
};

IN_PROC_BROWSER_TEST_F(DevToolsListenerBrowserTest, CanCollectCodeCoverage) {
  content::WebContents* web_contents = NavigateToTestFile("title2.html");

  constexpr char kEvalScript[] = "(function() { console.log('test') })()";
  ASSERT_TRUE(content::ExecJs(web_contents, kEvalScript));

  content::DevToolsAgentHost::RemoveObserver(this);
  content::RunAllTasksUntilIdle();

  CollectCodeCoverage("CanCollectCodeCoverage");

  content::DevToolsAgentHost::DetachAllClients();
  content::RunAllTasksUntilIdle();
}

}  // namespace coverage
