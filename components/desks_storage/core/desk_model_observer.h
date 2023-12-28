// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DESKS_STORAGE_CORE_DESK_MODEL_OBSERVER_H_
#define COMPONENTS_DESKS_STORAGE_CORE_DESK_MODEL_OBSERVER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/uuid.h"

namespace ash {
class DeskTemplate;
}

namespace desks_storage {

// Observer for the Desk model. In the observer methods care should
// be taken to not modify the model.
class DeskModelObserver {
 public:
  DeskModelObserver() = default;
  DeskModelObserver(const DeskModelObserver&) = delete;
  DeskModelObserver& operator=(const DeskModelObserver&) = delete;

  // Invoked when the model has finished loading. Until this method is called it
  // is unsafe to use the model.
  virtual void DeskModelLoaded() {}

  // Invoked when the model is about to be destroyed. Gives observes which may
  // outlive the model a chance to stop observing. Not pure virtual because it
  // needs to be called from the destructor.
  virtual void OnDeskModelDestroying() {}

  // Invoked when desk templates are added/updated, removed remotely via sync.
  // This is the mechanism for the sync server to push changes in the state of
  // the model to clients.
  virtual void EntriesAddedOrUpdatedRemotely(
      const std::vector<raw_ptr<const ash::DeskTemplate, VectorExperimental>>&
          new_entries) {}
  virtual void EntriesRemovedRemotely(const std::vector<base::Uuid>& uuids) {}

 protected:
  virtual ~DeskModelObserver() = default;
};

}  // namespace desks_storage

#endif  // COMPONENTS_DESKS_STORAGE_CORE_DESK_MODEL_OBSERVER_H_
