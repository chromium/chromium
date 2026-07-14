// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/notebooks/internal/notebooks_service_impl.h"

namespace notebooks {

NotebooksServiceImpl::NotebooksServiceImpl() = default;

NotebooksServiceImpl::~NotebooksServiceImpl() = default;

bool NotebooksServiceImpl::IsEmptyForTesting() const {
  return false;
}

}  // namespace notebooks
