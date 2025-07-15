// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_DETECTED_AGENT_CLIENT_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_DETECTED_AGENT_CLIENT_H_

#include <memory>
#include <vector>

#include "base/functional/callback_forward.h"
#include "components/device_signals/core/common/common_types.h"

namespace base {
class FilePath;
}  // namespace base

namespace device_signals {

// Client that can be used to retrieve information about security agents
// installed on the device.
class DetectedAgentClient {
 public:
  virtual ~DetectedAgentClient() = default;

  static std::unique_ptr<DetectedAgentClient> Create();

  // Will retrieve the detected agents of interests on the device. Will return
  // the value via `callback`.
  virtual void GetAgents(
      base::OnceCallback<void(std::vector<Agents>)> callback) = 0;

  // Sets a file path to be used for the agent install path.
  static void SetFilePathForTesting(const base::FilePath& file_path);
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_DETECTED_AGENT_CLIENT_H_
