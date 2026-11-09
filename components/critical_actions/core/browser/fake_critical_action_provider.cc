// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/critical_actions/core/browser/fake_critical_action_provider.h"

#include <utility>

#include "base/check.h"
#include "base/check_op.h"

namespace critical_actions {

FakeCriticalActionProvider::FakeCriticalActionProvider() = default;

FakeCriticalActionProvider::~FakeCriticalActionProvider() = default;

void FakeCriticalActionProvider::GetCriticalActions(
    const CriticalActionQueryOptions& options,
    Callback callback) {
  callbacks_.push_back(std::move(callback));
  if (on_request_) {
    std::move(on_request_).Run();
  }
}

base::WeakPtr<CriticalActionProvider> FakeCriticalActionProvider::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void FakeCriticalActionProvider::SetOnRequestCallback(
    base::OnceClosure callback) {
  CHECK(!on_request_);
  on_request_ = std::move(callback);
}

void FakeCriticalActionProvider::ResolveCallback(
    size_t index,
    std::vector<CriticalActionEntry> entries) {
  CHECK_LT(index, callbacks_.size());
  CHECK(callbacks_[index]);
  std::move(callbacks_[index]).Run(std::move(entries));
}

void FakeCriticalActionProvider::Shutdown() {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

}  // namespace critical_actions
