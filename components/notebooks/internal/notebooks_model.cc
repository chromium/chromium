// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/notebooks/internal/notebooks_model.h"

#include <optional>
#include <utility>

#include "base/auto_reset.h"
#include "base/check.h"
#include "base/containers/map_util.h"

namespace notebooks {

NotebooksModel::NotebooksModel() = default;

NotebooksModel::~NotebooksModel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool NotebooksModel::is_loaded() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return is_loaded_;
}

void NotebooksModel::SetLoaded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!is_notifying_);
  if (is_loaded_) {
    return;
  }
  is_loaded_ = true;
  base::AutoReset<bool> notifying(&is_notifying_, true);
  for (auto& observer : observers_) {
    observer.OnNotebooksModelLoaded();
  }
}

std::optional<Notebook> NotebooksModel::GetNotebook(
    const NotebookId& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (const Notebook* notebook = base::FindOrNull(notebooks_, id)) {
    return *notebook;
  }
  return std::nullopt;
}

std::vector<Notebook> NotebooksModel::GetAllNotebooks() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<Notebook> result;
  result.reserve(notebooks_.size());
  for (const auto& [id, notebook] : notebooks_) {
    result.push_back(notebook);
  }
  return result;
}

bool NotebooksModel::Contains(const NotebookId& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return notebooks_.contains(id);
}

void NotebooksModel::AddNotebook(Notebook notebook) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!is_notifying_);
  NotebookId id = notebook.id();
  CHECK(!Contains(id));

  auto it = notebooks_.emplace(id, std::move(notebook)).first;

  base::AutoReset<bool> notifying(&is_notifying_, true);
  for (auto& observer : observers_) {
    observer.OnNotebookAdded(it->second);
  }
}

void NotebooksModel::UpdateNotebook(Notebook notebook) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!is_notifying_);
  NotebookId id = notebook.id();
  auto it = notebooks_.find(id);
  CHECK(it != notebooks_.end());

  it->second = std::move(notebook);

  base::AutoReset<bool> notifying(&is_notifying_, true);
  for (auto& observer : observers_) {
    observer.OnNotebookUpdated(it->second);
  }
}

void NotebooksModel::AddOrUpdateNotebook(Notebook notebook) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (Contains(notebook.id())) {
    UpdateNotebook(std::move(notebook));
  } else {
    AddNotebook(std::move(notebook));
  }
}

void NotebooksModel::RemoveNotebook(NotebookId id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!is_notifying_);
  auto it = notebooks_.find(id);
  CHECK(it != notebooks_.end());

  notebooks_.erase(it);

  base::AutoReset<bool> notifying(&is_notifying_, true);
  for (auto& observer : observers_) {
    observer.OnNotebookRemoved(id);
  }
}

void NotebooksModel::AddObserver(NotebooksModelObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void NotebooksModel::RemoveObserver(NotebooksModelObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

}  // namespace notebooks
