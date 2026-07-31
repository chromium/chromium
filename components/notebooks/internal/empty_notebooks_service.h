// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NOTEBOOKS_INTERNAL_EMPTY_NOTEBOOKS_SERVICE_H_
#define COMPONENTS_NOTEBOOKS_INTERNAL_EMPTY_NOTEBOOKS_SERVICE_H_

#include "components/notebooks/public/notebooks_service.h"

namespace notebooks {

// An empty implementation of NotebooksService that can be used when the
// Notebooks feature is disabled.
class EmptyNotebooksService : public NotebooksService {
 public:
  EmptyNotebooksService();
  ~EmptyNotebooksService() override;

  // Disallow copy/assign.
  EmptyNotebooksService(const EmptyNotebooksService&) = delete;
  EmptyNotebooksService& operator=(const EmptyNotebooksService&) = delete;

  // NotebooksService:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  std::optional<Notebook> GetNotebook(const NotebookId& id) const override;
  std::vector<Notebook> GetAllNotebooks() const override;
  bool IsEmptyForTesting() const override;
  base::WeakPtr<syncer::DataTypeControllerDelegate> GetSyncControllerDelegate()
      override;
};

}  // namespace notebooks

#endif  // COMPONENTS_NOTEBOOKS_INTERNAL_EMPTY_NOTEBOOKS_SERVICE_H_
