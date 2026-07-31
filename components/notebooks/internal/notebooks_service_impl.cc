// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/notebooks/internal/notebooks_service_impl.h"

#include <utility>

namespace notebooks {

NotebooksServiceImpl::NotebooksServiceImpl(
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
    syncer::OnceDataTypeStoreFactory store_factory)
    : bridge_(&model_, std::move(change_processor), std::move(store_factory)) {
  model_.AddObserver(this);
}

NotebooksServiceImpl::~NotebooksServiceImpl() {
  model_.RemoveObserver(this);
}

void NotebooksServiceImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void NotebooksServiceImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void NotebooksServiceImpl::OnNotebookAdded(const Notebook& notebook) {
  observers_.Notify(&Observer::OnNotebookAdded, notebook);
}

void NotebooksServiceImpl::OnNotebookUpdated(const Notebook& notebook) {
  observers_.Notify(&Observer::OnNotebookUpdated, notebook);
}

void NotebooksServiceImpl::OnNotebookRemoved(const NotebookId& id) {
  observers_.Notify(&Observer::OnNotebookRemoved, id);
}

void NotebooksServiceImpl::OnNotebooksModelLoaded() {
  observers_.Notify(&Observer::OnNotebooksModelLoaded);
}

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
