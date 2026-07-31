// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NOTEBOOKS_PUBLIC_NOTEBOOKS_SERVICE_H_
#define COMPONENTS_NOTEBOOKS_PUBLIC_NOTEBOOKS_SERVICE_H_

#include <optional>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/observer_list_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/notebooks/public/notebook.h"
#include "components/notebooks/public/notebook_id.h"

namespace syncer {
class DataTypeControllerDelegate;
}  // namespace syncer

namespace notebooks {

// The core class for managing Notebooks.
class NotebooksService : public KeyedService {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override;

    // Called when notebook data finishes loading.
    virtual void OnNotebooksModelLoaded();

    // Called when a new Notebook is added.
    virtual void OnNotebookAdded(const Notebook& notebook);

    // Called when an existing Notebook is updated.
    virtual void OnNotebookUpdated(const Notebook& notebook);

    // Called when a Notebook is removed.
    virtual void OnNotebookRemoved(const NotebookId& id);
  };

  NotebooksService();
  ~NotebooksService() override;

  NotebooksService(const NotebooksService&) = delete;
  NotebooksService& operator=(const NotebooksService&) = delete;

  // Subscription management.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Model read accessors.
  virtual std::optional<Notebook> GetNotebook(const NotebookId& id) const = 0;
  virtual std::vector<Notebook> GetAllNotebooks() const = 0;

  // Returns true if this is the empty/no-op implementation.
  virtual bool IsEmptyForTesting() const = 0;

  // Serves as the plumbing entry point for Chrome Sync, bridging the Sync
  // engine to the internal sync bridge to manage lifecycle events (e.g.,
  // startup, stop) and propagate sync updates.
  virtual base::WeakPtr<syncer::DataTypeControllerDelegate>
  GetSyncControllerDelegate() = 0;
};

}  // namespace notebooks

#endif  // COMPONENTS_NOTEBOOKS_PUBLIC_NOTEBOOKS_SERVICE_H_
