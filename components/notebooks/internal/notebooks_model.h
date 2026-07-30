// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NOTEBOOKS_INTERNAL_NOTEBOOKS_MODEL_H_
#define COMPONENTS_NOTEBOOKS_INTERNAL_NOTEBOOKS_MODEL_H_

#include <optional>
#include <vector>

#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "components/notebooks/internal/notebooks_model_observer.h"
#include "components/notebooks/public/notebook.h"
#include "components/notebooks/public/notebook_id.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace notebooks {

// Manages the in-memory collection of Notebook objects for the active profile
// session. Dispatches model change notifications to registered observers.
class NotebooksModel {
 public:
  NotebooksModel();
  ~NotebooksModel();

  NotebooksModel(const NotebooksModel&) = delete;
  NotebooksModel& operator=(const NotebooksModel&) = delete;

  // Returns true if the model has finished initial loading.
  bool is_loaded() const;

  // Marks the model as loaded and notifies observers.
  void SetLoaded();

  // Model accessors.
  std::optional<Notebook> GetNotebook(const NotebookId& id) const;
  std::vector<Notebook> GetAllNotebooks() const;
  bool Contains(const NotebookId& id) const;

  // Model mutators.
  void AddNotebook(Notebook notebook);
  void UpdateNotebook(Notebook notebook);
  void AddOrUpdateNotebook(Notebook notebook);
  void RemoveNotebook(NotebookId id);

  // Observer management.
  void AddObserver(NotebooksModelObserver* observer);
  void RemoveObserver(NotebooksModelObserver* observer);

 private:
  bool is_loaded_ = false;
  bool is_notifying_ = false;

  // Primary storage map indexed by Notebook ID.
  absl::flat_hash_map<NotebookId, Notebook, NotebookIdHash> notebooks_;

  base::ObserverList<NotebooksModelObserver> observers_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace notebooks

#endif  // COMPONENTS_NOTEBOOKS_INTERNAL_NOTEBOOKS_MODEL_H_
