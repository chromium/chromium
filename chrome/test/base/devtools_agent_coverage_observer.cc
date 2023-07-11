// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/devtools_agent_coverage_observer.h"

#include "content/public/test/browser_test_utils.h"

DevToolsAgentCoverageObserver::DevToolsAgentCoverageObserver(
    base::FilePath devtools_code_coverage_dir)
    : devtools_code_coverage_dir_(devtools_code_coverage_dir) {
  content::DevToolsAgentHost::AddObserver(this);
}

DevToolsAgentCoverageObserver::DevToolsAgentCoverageObserver(
    base::FilePath devtools_code_coverage_dir,
    ShouldInspectDevToolsAgentHostCallback should_inspect_callback)
    : DevToolsAgentCoverageObserver(devtools_code_coverage_dir) {
  should_inspect_callback_ = std::move(should_inspect_callback);
}

DevToolsAgentCoverageObserver::~DevToolsAgentCoverageObserver() = default;

bool DevToolsAgentCoverageObserver::CoverageEnabled() {
  return !devtools_code_coverage_dir_.empty();
}

void DevToolsAgentCoverageObserver::CollectCoverage(
    const std::string& test_name) {
  ASSERT_TRUE(CoverageEnabled());

  content::DevToolsAgentHost::RemoveObserver(this);
  content::RunAllTasksUntilIdle();

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath store =
      devtools_code_coverage_dir_.AppendASCII("webui_javascript_code_coverage");
  coverage::DevToolsListener::SetupCoverageStore(store);

  for (auto& agent : devtools_agents_) {
    auto* host = agent.first;
    if (agent.second->HasCoverage(host))
      agent.second->GetCoverage(host, store, test_name);
    agent.second->Detach(host);
  }

  content::DevToolsAgentHost::DetachAllClients();
}

bool DevToolsAgentCoverageObserver::ShouldForceDevToolsAgentHostCreation() {
  return CoverageEnabled();
}

void DevToolsAgentCoverageObserver::DevToolsAgentHostCreated(
    content::DevToolsAgentHost* host) {
  CHECK(devtools_agents_.find(host) == devtools_agents_.end());

  if (!should_inspect_callback_.is_null() &&
      !should_inspect_callback_.Run(host)) {
    return;
  }

  uint32_t process_id = base::GetUniqueIdForProcess().GetUnsafeValue();
  devtools_agents_[host] =
      std::make_unique<coverage::DevToolsListener>(host, process_id);
}

void DevToolsAgentCoverageObserver::DevToolsAgentHostAttached(
    content::DevToolsAgentHost* host) {}

void DevToolsAgentCoverageObserver::DevToolsAgentHostNavigated(
    content::DevToolsAgentHost* host) {
  if (devtools_agents_.find(host) == devtools_agents_.end())
    return;

  devtools_agents_.find(host)->second->Navigated(host);
}

void DevToolsAgentCoverageObserver::DevToolsAgentHostDetached(
    content::DevToolsAgentHost* host) {}

void DevToolsAgentCoverageObserver::DevToolsAgentHostCrashed(
    content::DevToolsAgentHost* host,
    base::TerminationStatus status) {
  ASSERT_TRUE(devtools_agents_.find(host) == devtools_agents_.end());
}
