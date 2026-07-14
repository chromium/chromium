// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NOTEBOOKS_INTERNAL_NOTEBOOKS_SERVICE_IMPL_H_
#define COMPONENTS_NOTEBOOKS_INTERNAL_NOTEBOOKS_SERVICE_IMPL_H_

#include "components/notebooks/public/notebooks_service.h"

namespace notebooks {

// The internal implementation of the NotebooksService.
class NotebooksServiceImpl : public NotebooksService {
 public:
  NotebooksServiceImpl();
  ~NotebooksServiceImpl() override;

  // Disallow copy/assign.
  NotebooksServiceImpl(const NotebooksServiceImpl&) = delete;
  NotebooksServiceImpl& operator=(const NotebooksServiceImpl&) = delete;

  // NotebooksService:
  bool IsEmptyForTesting() const override;
};

}  // namespace notebooks

#endif  // COMPONENTS_NOTEBOOKS_INTERNAL_NOTEBOOKS_SERVICE_IMPL_H_
