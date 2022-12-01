// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_DEVTOOLS_AGENT_COVERAGE_OBSERVER_H_
#define CHROME_TEST_BASE_DEVTOOLS_AGENT_COVERAGE_OBSERVER_H_

#include "chrome/test/base/devtools_listener.h"
#include "content/public/browser/devtools_agent_host_observer.h"

// Observes new DevToolsAgentHosts and ensures code coverage is enabled and
// can be collected.
class DevToolsAgentCoverageObserver
    : public content::DevToolsAgentHostObserver {
 public:
  explicit DevToolsAgentCoverageObserver(
      base::FilePath devtools_code_coverage_dir);
  ~DevToolsAgentCoverageObserver() override;

  bool CoverageEnabled();
  void CollectCoverage(const std::string& test_name);

 protected:
  // content::DevToolsAgentHostObserver
  bool ShouldForceDevToolsAgentHostCreation() override;
  void DevToolsAgentHostCreated(content::DevToolsAgentHost* host) override;
  void DevToolsAgentHostAttached(content::DevToolsAgentHost* host) override;
  void DevToolsAgentHostNavigated(content::DevToolsAgentHost* host) override;
  void DevToolsAgentHostDetached(content::DevToolsAgentHost* host) override;
  void DevToolsAgentHostCrashed(content::DevToolsAgentHost* host,
                                base::TerminationStatus status) override;

 private:
  using DevToolsAgentMap =  // agent hosts: have a unique devtools listener
      std::map<content::DevToolsAgentHost*,
               std::unique_ptr<coverage::DevToolsListener>>;
  base::FilePath devtools_code_coverage_dir_;
  DevToolsAgentMap devtools_agents_;
};

#endif  // CHROME_TEST_BASE_DEVTOOLS_AGENT_COVERAGE_OBSERVER_H_
