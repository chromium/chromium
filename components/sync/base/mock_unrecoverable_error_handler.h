// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_MOCK_UNRECOVERABLE_ERROR_HANDLER_H_
#define COMPONENTS_SYNC_BASE_MOCK_UNRECOVERABLE_ERROR_HANDLER_H_

#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/sync/base/unrecoverable_error_handler.h"

namespace syncer {

// Mock implementation of UnrecoverableErrorHandler that counts how many times
// it has been invoked.
class MockUnrecoverableErrorHandler : public UnrecoverableErrorHandler {
 public:
  MockUnrecoverableErrorHandler();
  ~MockUnrecoverableErrorHandler() override;
  void OnUnrecoverableError(const base::Location& from_here,
                            const std::string& message) override;

  // Returns the number of times this handler has been invoked.
  int invocation_count() const;

  base::WeakPtr<MockUnrecoverableErrorHandler> GetWeakPtr();

 private:
  int invocation_count_;

  base::WeakPtrFactory<MockUnrecoverableErrorHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MockUnrecoverableErrorHandler);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_MOCK_UNRECOVERABLE_ERROR_HANDLER_H_
