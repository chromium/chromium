// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/mock_unrecoverable_error_handler.h"

namespace syncer {

MockUnrecoverableErrorHandler::MockUnrecoverableErrorHandler()
    : invocation_count_(0) {}

MockUnrecoverableErrorHandler::~MockUnrecoverableErrorHandler() {}

void MockUnrecoverableErrorHandler::OnUnrecoverableError(
    const base::Location& from_here,
    const std::string& message) {
  ++invocation_count_;
}

int MockUnrecoverableErrorHandler::invocation_count() const {
  return invocation_count_;
}

base::WeakPtr<MockUnrecoverableErrorHandler>
MockUnrecoverableErrorHandler::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace syncer
