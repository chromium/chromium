// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_RECORD_REPLAY_CORE_BROWSER_TASK_PROVIDER_H_
#define COMPONENTS_RECORD_REPLAY_CORE_BROWSER_TASK_PROVIDER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "components/record_replay/core/browser/task_discovery_service.h"

class GURL;

namespace record_replay {

// Interface for providers that supply automation tasks for specific URLs.
// Implementations can be backed by hard-coded data or local databases.
class TaskProvider {
 public:
  virtual ~TaskProvider() = default;

  // Asynchronously determines if the given URL is eligible for suggestions.
  // Runs the callback with the metadata if eligible, or nullopt otherwise.
  virtual void ShouldOfferTask(
      const GURL& url,
      base::OnceCallback<
          void(std::optional<TaskDiscoveryService::AutomationMetadata>)>
          callback) = 0;

  // Creates the set of available task providers.
  // Returns multiple providers to support fallback chains.
  static std::vector<std::unique_ptr<TaskProvider>> CreateProviders();
};

}  // namespace record_replay

#endif  // COMPONENTS_RECORD_REPLAY_CORE_BROWSER_TASK_PROVIDER_H_
