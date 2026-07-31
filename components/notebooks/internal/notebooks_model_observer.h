// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NOTEBOOKS_INTERNAL_NOTEBOOKS_MODEL_OBSERVER_H_
#define COMPONENTS_NOTEBOOKS_INTERNAL_NOTEBOOKS_MODEL_OBSERVER_H_

#include "base/observer_list_types.h"
#include "components/notebooks/public/notebook.h"
#include "components/notebooks/public/notebook_id.h"

namespace notebooks {

// Observer interface for monitoring changes to the NotebooksModel. Observers
// must not synchronously mutate the model from notification methods.
class NotebooksModelObserver : public base::CheckedObserver {
 public:
  ~NotebooksModelObserver() override;

  // Called when the model finishes loading data from storage or sync.
  virtual void OnNotebooksModelLoaded();

  // Called when a new Notebook is added to the model.
  virtual void OnNotebookAdded(const Notebook& notebook);

  // Called when an existing Notebook is updated in the model.
  virtual void OnNotebookUpdated(const Notebook& notebook);

  // Called when a Notebook is removed from the model.
  virtual void OnNotebookRemoved(const NotebookId& id);
};

}  // namespace notebooks

#endif  // COMPONENTS_NOTEBOOKS_INTERNAL_NOTEBOOKS_MODEL_OBSERVER_H_
