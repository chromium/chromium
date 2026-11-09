// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRITICAL_ACTIONS_CORE_BROWSER_FAKE_CRITICAL_ACTION_PROVIDER_H_
#define COMPONENTS_CRITICAL_ACTIONS_CORE_BROWSER_FAKE_CRITICAL_ACTION_PROVIDER_H_

#include <cstddef>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/critical_actions/core/browser/critical_action_provider.h"
#include "components/critical_actions/core/browser/critical_action_types.h"

namespace critical_actions {

// A CriticalActionProvider for tests that holds on to every
// GetCriticalActions() request until the test explicitly resolves it, so tests
// can control when (and in which order) lookups complete.
class FakeCriticalActionProvider : public CriticalActionProvider {
 public:
  using Callback = base::OnceCallback<void(std::vector<CriticalActionEntry>)>;

  FakeCriticalActionProvider();
  FakeCriticalActionProvider(const FakeCriticalActionProvider&) = delete;
  FakeCriticalActionProvider& operator=(const FakeCriticalActionProvider&) =
      delete;
  ~FakeCriticalActionProvider() override;

  // CriticalActionProvider:
  void GetCriticalActions(const CriticalActionQueryOptions& options,
                          Callback callback) override;
  base::WeakPtr<CriticalActionProvider> GetWeakPtr() override;

  // Runs `callback` once, the next time GetCriticalActions() is called. Must
  // not be called again before that.
  void SetOnRequestCallback(base::OnceClosure callback);

  size_t request_count() const { return callbacks_.size(); }

  // Completes the `index`-th GetCriticalActions() request with `entries`.
  void ResolveCallback(size_t index, std::vector<CriticalActionEntry> entries);

  // Invalidates weak pointers handed out by GetWeakPtr(), like
  // CriticalActionService::Shutdown() does.
  void Shutdown();

 private:
  std::vector<Callback> callbacks_;
  base::OnceClosure on_request_;
  base::WeakPtrFactory<FakeCriticalActionProvider> weak_ptr_factory_{this};
};

}  // namespace critical_actions

#endif  // COMPONENTS_CRITICAL_ACTIONS_CORE_BROWSER_FAKE_CRITICAL_ACTION_PROVIDER_H_
