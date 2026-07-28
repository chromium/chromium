// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NOTEBOOKS_PUBLIC_NOTEBOOKS_SERVICE_H_
#define COMPONENTS_NOTEBOOKS_PUBLIC_NOTEBOOKS_SERVICE_H_

#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

namespace syncer {
class DataTypeControllerDelegate;
}  // namespace syncer

namespace notebooks {

// The core class for managing Notebooks.
class NotebooksService : public KeyedService {
 public:
  NotebooksService() = default;
  ~NotebooksService() override = default;

  NotebooksService(const NotebooksService&) = delete;
  NotebooksService& operator=(const NotebooksService&) = delete;

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
