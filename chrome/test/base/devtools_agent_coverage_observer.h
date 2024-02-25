// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_DEVTOOLS_AGENT_COVERAGE_OBSERVER_H_
#define CHROME_TEST_BASE_DEVTOOLS_AGENT_COVERAGE_OBSERVER_H_

#include <map>
#include <memory>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "chrome/test/base/devtools_listener.h"
#include "content/public/browser/devtools_agent_host_observer.h"

// Callback to filter out the `DevToolsAgentHost*` that get attached to.
// Extensions with background / foreground pages share the same v8 isolate
// (see crbug.com/v8/10820) so don't get coverage for one of those to avoid a
// DCHECK.
using ShouldInspectDevToolsAgentHostCallback =
    base::RepeatingCallback<bool(content::DevToolsAgentHost*)>;

// Observes new DevToolsAgentHosts and ensures code coverage is enabled and
// can be collected.
class DevToolsAgentCoverageObserver
    : public content::DevToolsAgentHostObserver {
 public:
  explicit DevToolsAgentCoverageObserver(
      base::FilePath devtools_code_coverage_dir);
  DevToolsAgentCoverageObserver(
      base::FilePath devtools_code_coverage_dir,
      ShouldInspectDevToolsAgentHostCallback should_inspect_callback);
  ~DevToolsAgentCoverageObserver() override;

  bool CoverageEnabled() const;
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
  void DevToolsAgentHostDestroyed(
      content::DevToolsAgentHost* agent_host) override;

 private:
  using DevToolsAgentMap =  // agent hosts: have a unique devtools listener
      std::map<content::DevToolsAgentHost*,
               std::pair<scoped_refptr<content::DevToolsAgentHost>,
                         std::unique_ptr<coverage::DevToolsListener>>>;
  base::FilePath devtools_code_coverage_dir_;
  DevToolsAgentMap devtools_agents_;
  ShouldInspectDevToolsAgentHostCallback should_inspect_callback_;
};

#endif  // CHROME_TEST_BASE_DEVTOOLS_AGENT_COVERAGE_OBSERVER_H_
