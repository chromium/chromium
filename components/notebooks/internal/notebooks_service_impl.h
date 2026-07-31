// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NOTEBOOKS_INTERNAL_NOTEBOOKS_SERVICE_IMPL_H_
#define COMPONENTS_NOTEBOOKS_INTERNAL_NOTEBOOKS_SERVICE_IMPL_H_

#include <memory>

#include "base/observer_list.h"
#include "components/notebooks/internal/notebook_sync_bridge.h"
#include "components/notebooks/internal/notebooks_model.h"
#include "components/notebooks/public/notebooks_service.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/model/data_type_store.h"

namespace notebooks {

// The internal implementation of the NotebooksService.
class NotebooksServiceImpl : public NotebooksService,
                             public NotebooksModelObserver {
 public:
  NotebooksServiceImpl(
      std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
      syncer::OnceDataTypeStoreFactory store_factory);
  ~NotebooksServiceImpl() override;

  // Disallow copy/assign.
  NotebooksServiceImpl(const NotebooksServiceImpl&) = delete;
  NotebooksServiceImpl& operator=(const NotebooksServiceImpl&) = delete;

  // NotebooksService:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  std::optional<Notebook> GetNotebook(const NotebookId& id) const override;
  std::vector<Notebook> GetAllNotebooks() const override;
  bool IsEmptyForTesting() const override;
  base::WeakPtr<syncer::DataTypeControllerDelegate> GetSyncControllerDelegate()
      override;

  // NotebooksModelObserver:
  void OnNotebookAdded(const Notebook& notebook) override;
  void OnNotebookUpdated(const Notebook& notebook) override;
  void OnNotebookRemoved(const NotebookId& id) override;
  void OnNotebooksModelLoaded() override;

 private:
  base::ObserverList<Observer> observers_;
  NotebooksModel model_;
  NotebookSyncBridge bridge_;
};

}  // namespace notebooks

#endif  // COMPONENTS_NOTEBOOKS_INTERNAL_NOTEBOOKS_SERVICE_IMPL_H_
