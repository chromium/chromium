// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_DEVTOOLS_LISTENER_H_
#define CHROME_TEST_BASE_DEVTOOLS_LISTENER_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_agent_host_client.h"

namespace coverage {

// Collects code coverage from a WebContents, during a browser test
// for example, using Chrome Devtools Protocol (CDP).
class DevToolsListener : public content::DevToolsAgentHostClient {
 public:
  // Attaches to host and enables CDP.
  DevToolsListener(content::DevToolsAgentHost* host, uint32_t uuid);
  ~DevToolsListener() override;

  // Host navigation starts code coverage.
  void Navigated(content::DevToolsAgentHost* host);

  // Returns true if host has started code coverage.
  bool HasCoverage(content::DevToolsAgentHost* host);

  // If host HasCoverage(), collect it and save it in |store|.
  void GetCoverage(content::DevToolsAgentHost* host,
                   const base::FilePath& store,
                   const std::string& test);

  // Detaches from host.
  void Detach(content::DevToolsAgentHost* host);

  // Returns a unique host identifier, with optional |prefix|.
  static std::string HostString(content::DevToolsAgentHost* host,
                                const std::string& prefix = {});

  // Creates coverage output directory and subdirectories.
  static void SetupCoverageStore(const base::FilePath& store_path);

 private:
  // Starts CDP session on host.
  void Start(content::DevToolsAgentHost* host);

  // Starts JavaScript (JS) code coverage on host.
  bool StartJSCoverage(content::DevToolsAgentHost* host);

  // Collects JavaScript coverage from host and saves it in |store|.
  void StopAndStoreJSCoverage(content::DevToolsAgentHost* host,
                              const base::FilePath& store,
                              const std::string& test);

  // Stores JS scripts used during code execution on host.
  void StoreScripts(content::DevToolsAgentHost* host,
                    const base::FilePath& store);

  // Sends CDP commands to host.
  void SendCommandMessage(content::DevToolsAgentHost* host,
                          const std::string& command);

  // Awaits CDP response to command `id` and returns false if the host was
  // closed whilst waiting, true otherwise.
  bool AwaitCommandResponse(int id);

  // Receives CDP messages from host.
  void DispatchProtocolMessage(content::DevToolsAgentHost* host,
                               base::span<const uint8_t> message) override;

  // Returns true if URL should be attached to.
  bool MayAttachToURL(const GURL& url, bool is_webui) override;

  // Called if host was shut down (closed).
  void AgentHostClosed(content::DevToolsAgentHost* host) override;

  // Repeatedly verify all the script IDs from the coverage entries are
  // available and call `finished_callback` on completion (either retries
  // exhausted or all scripts are available).
  void VerifyAllScriptsAreParsedRepeatedly(
      const base::Value::List* coverage_entries,
      base::OnceClosure done_callback,
      int retries);

  std::vector<base::Value::Dict> scripts_;
  base::Value::Dict script_coverage_;
  std::map<std::string, std::string> script_hash_map_;
  std::map<std::string, std::string> script_id_map_;

  base::OnceClosure value_closure_;
  base::Value::Dict value_;
  int value_id_ = 0;

  const std::string uuid_;
  bool navigated_ = false;
  bool attached_ = true;

  bool all_scripts_parsed_ = false;

  base::WeakPtrFactory<DevToolsListener> weak_ptr_factory_{this};
};

}  // namespace coverage

#endif  // CHROME_TEST_BASE_DEVTOOLS_LISTENER_H_
