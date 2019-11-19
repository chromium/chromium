// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/test_unrecoverable_error_handler.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

TestUnrecoverableErrorHandler::TestUnrecoverableErrorHandler() {}

TestUnrecoverableErrorHandler::~TestUnrecoverableErrorHandler() {}

void TestUnrecoverableErrorHandler::OnUnrecoverableError(
    const base::Location& from_here,
    const std::string& message) {
  ADD_FAILURE_AT(from_here.file_name(), from_here.line_number())
      << from_here.function_name() << ": " << message;
}

base::WeakPtr<TestUnrecoverableErrorHandler>
TestUnrecoverableErrorHandler::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace syncer
