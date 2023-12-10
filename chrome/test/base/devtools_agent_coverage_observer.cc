// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/devtools_agent_coverage_observer.h"

#include <ostream>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "content/public/test/browser_test_utils.h"

namespace {

using content::DevToolsAgentHost;
using coverage::DevToolsListener;

std::ostream& operator<<(std::ostream& out, DevToolsAgentHost* h) {
  return out << "Host {title: '"
             << (h->GetTitle().empty() ? "none" : h->GetTitle()) << "', URL: '"
             << (h->GetURL().is_empty() ? "none" : h->GetURL().spec())
             << "', ID: '" << (h->GetId().empty() ? "none" : h->GetId())
             << "'}";
}

}  // namespace

DevToolsAgentCoverageObserver::DevToolsAgentCoverageObserver(
    base::FilePath devtools_code_coverage_dir)
    : DevToolsAgentCoverageObserver(devtools_code_coverage_dir,
                                    base::NullCallback()) {}

DevToolsAgentCoverageObserver::DevToolsAgentCoverageObserver(
    base::FilePath devtools_code_coverage_dir,
    ShouldInspectDevToolsAgentHostCallback should_inspect_callback)
    : devtools_code_coverage_dir_(devtools_code_coverage_dir),
      should_inspect_callback_(std::move(should_inspect_callback)) {
  DevToolsAgentHost::AddObserver(this);
}

DevToolsAgentCoverageObserver::~DevToolsAgentCoverageObserver() = default;

bool DevToolsAgentCoverageObserver::CoverageEnabled() const {
  return !devtools_code_coverage_dir_.empty();
}

void DevToolsAgentCoverageObserver::CollectCoverage(
    const std::string& test_name) {
  CHECK(CoverageEnabled());

  DevToolsAgentHost::RemoveObserver(this);
  content::RunAllTasksUntilIdle();

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath store =
      devtools_code_coverage_dir_.AppendASCII("webui_javascript_code_coverage");
  DevToolsListener::SetupCoverageStore(store);

  VLOG(1) << "Collecting coverage for " << devtools_agents_.size() << " agents";
  for (auto& agent : devtools_agents_) {
    DevToolsAgentHost* host = agent.first;
    DevToolsListener* listener = agent.second.second.get();
    if (listener->HasCoverage(host)) {
      VLOG(1) << host << ": Collecting coverage";
      listener->GetCoverage(host, store, test_name);
    }
    VLOG(1) << host << ": Detaching";
    listener->Detach(host);
  }

  DevToolsAgentHost::DetachAllClients();
}

bool DevToolsAgentCoverageObserver::ShouldForceDevToolsAgentHostCreation() {
  return CoverageEnabled();
}

void DevToolsAgentCoverageObserver::DevToolsAgentHostCreated(
    DevToolsAgentHost* host) {
  VLOG(1) << host << ": Created";
  CHECK(!base::Contains(devtools_agents_, host));

  if (should_inspect_callback_ && !should_inspect_callback_.Run(host)) {
    VLOG(1) << host << ": Not attaching";
    return;
  }

  uint32_t process_id = base::GetUniqueIdForProcess().GetUnsafeValue();
  devtools_agents_[host] =
      std::make_pair(base::WrapRefCounted(host),
                     std::make_unique<DevToolsListener>(host, process_id));
}

void DevToolsAgentCoverageObserver::DevToolsAgentHostAttached(
    DevToolsAgentHost* host) {
  VLOG(1) << host << ": Attached";
}

void DevToolsAgentCoverageObserver::DevToolsAgentHostNavigated(
    DevToolsAgentHost* host) {
  VLOG(1) << host << ": Navigated";
  const auto& it = devtools_agents_.find(host);
  if (it == devtools_agents_.end()) {
    VLOG(1) << host << ": Not found found in agent map";
    return;
  }

  it->second.second->Navigated(host);
}

void DevToolsAgentCoverageObserver::DevToolsAgentHostDetached(
    DevToolsAgentHost* host) {
  VLOG(1) << host << ": Detached";
}

void DevToolsAgentCoverageObserver::DevToolsAgentHostCrashed(
    DevToolsAgentHost* host,
    base::TerminationStatus status) {
  VLOG(1) << host << ": Crashed";
  CHECK(!base::Contains(devtools_agents_, host));
}

void DevToolsAgentCoverageObserver::DevToolsAgentHostDestroyed(
    content::DevToolsAgentHost* host) {
  // At this point the ID is only available the other elements in the
  // `std::ostream` have been destroyed.
  VLOG(1) << host->GetId() << ": Destroyed";
}
