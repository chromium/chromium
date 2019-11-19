// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_TEST_UNRECOVERABLE_ERROR_HANDLER_H_
#define COMPONENTS_SYNC_BASE_TEST_UNRECOVERABLE_ERROR_HANDLER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "components/sync/base/unrecoverable_error_handler.h"

namespace syncer {

// Implementation of UnrecoverableErrorHandler that simply adds a
// gtest failure.
class TestUnrecoverableErrorHandler : public UnrecoverableErrorHandler {
 public:
  TestUnrecoverableErrorHandler();
  ~TestUnrecoverableErrorHandler() override;

  void OnUnrecoverableError(const base::Location& from_here,
                            const std::string& message) override;

  base::WeakPtr<TestUnrecoverableErrorHandler> GetWeakPtr();

 private:
  base::WeakPtrFactory<TestUnrecoverableErrorHandler> weak_ptr_factory_{this};
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_TEST_UNRECOVERABLE_ERROR_HANDLER_H_
