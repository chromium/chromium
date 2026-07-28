// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/notebooks/internal/empty_notebooks_service.h"

#include "base/notreached.h"

namespace notebooks {

EmptyNotebooksService::EmptyNotebooksService() = default;

EmptyNotebooksService::~EmptyNotebooksService() = default;

bool EmptyNotebooksService::IsEmptyForTesting() const {
  return true;
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
EmptyNotebooksService::GetSyncControllerDelegate() {
  NOTREACHED();
}
}  // namespace notebooks
