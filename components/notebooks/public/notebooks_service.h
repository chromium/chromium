// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NOTEBOOKS_PUBLIC_NOTEBOOKS_SERVICE_H_
#define COMPONENTS_NOTEBOOKS_PUBLIC_NOTEBOOKS_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"

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
};

}  // namespace notebooks

#endif  // COMPONENTS_NOTEBOOKS_PUBLIC_NOTEBOOKS_SERVICE_H_
