// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/notebooks/internal/empty_notebooks_service.h"

#include "base/notreached.h"

namespace notebooks {

EmptyNotebooksService::EmptyNotebooksService() = default;

EmptyNotebooksService::~EmptyNotebooksService() = default;

void EmptyNotebooksService::AddObserver(Observer* observer) {}

void EmptyNotebooksService::RemoveObserver(Observer* observer) {}

bool EmptyNotebooksService::IsEmptyForTesting() const {
  return true;
}

bool EmptyNotebooksService::IsUserEligible() const {
  return false;
}

bool EmptyNotebooksService::IsEligibilityLoading() const {
  return false;
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
EmptyNotebooksService::GetSyncControllerDelegate() {
  NOTREACHED();
}
}  // namespace notebooks
