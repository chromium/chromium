// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/zero_state_sync_controller.h"

#include "chrome/browser/ui/app_list/search/search_controller_impl_new.h"

namespace app_list {

ZeroStateSyncController::ZeroStateSyncController(
    SearchControllerImplNew* search_controller)
    : search_controller_(search_controller) {}

ZeroStateSyncController::~ZeroStateSyncController() {}

void ZeroStateSyncController::AddProvider(SearchProvider* provider) {
  if (provider && provider->ShouldBlockZeroState())
    ++total_blockers_;
}

void ZeroStateSyncController::Start(base::TimeDelta timeout,
                                    base::OnceClosure on_done) {
  on_done_ = std::move(on_done);
  returned_blockers_ = 0;

  timeout_.Start(FROM_HERE, timeout,
                 base::BindOnce(&ZeroStateSyncController::OnZeroStateTimedOut,
                                base::Unretained(this)));
}

void ZeroStateSyncController::Stop() {
  // Cancel a pending zero-state publish if it exists.
  timeout_.Stop();
}

void ZeroStateSyncController::UpdateResults(const SearchProvider* provider) {
  if (provider->ShouldBlockZeroState())
    ++returned_blockers_;

  if (!on_done_) {
    // Zero-state has been unblocked, publish immediately.
    search_controller_->Publish();
  } else if (returned_blockers_ == total_blockers_) {
    // All zero-state blockers have returned. Publish everything received so
    // far, and trigger the on-done callback.
    search_controller_->Publish();
    std::move(on_done_.value()).Run();
    on_done_.reset();
  }
}

void ZeroStateSyncController::OnZeroStateTimedOut() {
  // This will be nullopt if all zero-state blocking providers have returned. If
  // it isn't, publish whatever results have been returned.
  if (on_done_.has_value()) {
    search_controller_->Publish();
    std::move(on_done_.value()).Run();
    on_done_.reset();
  }
}

}  // namespace app_list
