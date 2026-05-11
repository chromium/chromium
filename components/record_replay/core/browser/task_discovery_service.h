// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_RECORD_REPLAY_CORE_BROWSER_TASK_DISCOVERY_SERVICE_H_
#define COMPONENTS_RECORD_REPLAY_CORE_BROWSER_TASK_DISCOVERY_SERVICE_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"

class GURL;

namespace record_replay {

// Service that discovers if automation tasks are available for a given
// URL. Lifetime is typically scoped to a Tab (via TabFeature).
class TaskDiscoveryService {
 public:
  struct AutomationMetadata {
    // The text displayed on the entry point button (the nudge label).
    // E.g., "Speed up task?".
    std::string title;

    // The instructions sent to the actor when the offering is accepted.
    std::string instructions;

    // The tooltip message text shown anchored to the entry point button.
    // Note: The feature flag `glic::kUseAnchoredMessage` must be enabled for
    // this to be visible and auto-submit the instructions.
    std::string anchored_message;
  };

  virtual ~TaskDiscoveryService() = default;

  // Asynchronously determines if the given URL is eligible for suggestions.
  // Runs the callback with the result.
  virtual void ShouldOfferTask(const GURL& url,
                               base::OnceCallback<void(bool)> callback) = 0;

  // Returns visual metadata strings associated with the automation offer,
  // or nullopt if no offer is available.
  virtual std::optional<AutomationMetadata> GetMetadata() = 0;
};

}  // namespace record_replay

#endif  // COMPONENTS_RECORD_REPLAY_CORE_BROWSER_TASK_DISCOVERY_SERVICE_H_
