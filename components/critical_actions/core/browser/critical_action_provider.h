// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRITICAL_ACTIONS_CORE_BROWSER_CRITICAL_ACTION_PROVIDER_H_
#define COMPONENTS_CRITICAL_ACTIONS_CORE_BROWSER_CRITICAL_ACTION_PROVIDER_H_

#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/critical_actions/core/browser/critical_action_types.h"

namespace critical_actions {

// Interface for querying critical action records without depending on
// CriticalActionService (which observes history::HistoryService).
class CriticalActionProvider {
 public:
  virtual ~CriticalActionProvider() = default;

  // Retrieves critical action records matching `options`, sorted by
  // `timestamp` descending (latest first).
  virtual void GetCriticalActions(
      const CriticalActionQueryOptions& options,
      base::OnceCallback<void(std::vector<CriticalActionEntry>)> callback) = 0;

  // Returns a WeakPtr that is invalidated when the underlying service shuts
  // down.
  virtual base::WeakPtr<CriticalActionProvider> GetWeakPtr() = 0;
};

}  // namespace critical_actions

#endif  // COMPONENTS_CRITICAL_ACTIONS_CORE_BROWSER_CRITICAL_ACTION_PROVIDER_H_
