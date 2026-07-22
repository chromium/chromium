// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/notebooks/internal/notebooks_service_impl.h"

#include <utility>

namespace notebooks {

NotebooksServiceImpl::NotebooksServiceImpl(
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
    syncer::OnceDataTypeStoreFactory store_factory)
    : bridge_(std::move(change_processor), std::move(store_factory)) {}

NotebooksServiceImpl::~NotebooksServiceImpl() = default;

void NotebooksServiceImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void NotebooksServiceImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool NotebooksServiceImpl::IsEmptyForTesting() const {
  return false;
}

bool NotebooksServiceImpl::IsUserEligible() const {
  // Stub implementation: eligibility checks will be added in subsequent CLs.
  return false;
}

bool NotebooksServiceImpl::IsEligibilityLoading() const {
  // Stub implementation: eligibility is not loading.
  return false;
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
NotebooksServiceImpl::GetSyncControllerDelegate() {
  return bridge_.change_processor()->GetControllerDelegate();
}
}  // namespace notebooks
