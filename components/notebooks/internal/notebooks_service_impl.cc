// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/notebooks/internal/notebooks_service_impl.h"

#include <utility>

namespace notebooks {

NotebooksServiceImpl::NotebooksServiceImpl(
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
    syncer::OnceDataTypeStoreFactory store_factory)
    : bridge_(&model_, std::move(change_processor), std::move(store_factory)) {}

NotebooksServiceImpl::~NotebooksServiceImpl() = default;

std::optional<Notebook> NotebooksServiceImpl::GetNotebook(
    const NotebookId& id) const {
  return model_.GetNotebook(id);
}

std::vector<Notebook> NotebooksServiceImpl::GetAllNotebooks() const {
  return model_.GetAllNotebooks();
}

bool NotebooksServiceImpl::IsEmptyForTesting() const {
  return false;
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
NotebooksServiceImpl::GetSyncControllerDelegate() {
  return bridge_.change_processor()->GetControllerDelegate();
}

}  // namespace notebooks
