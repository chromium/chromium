// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/update_client/pipeline_util.h"

#include "base/files/file_path.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/update_client/protocol_definition.h"
#include "components/update_client/update_client_errors.h"

namespace update_client {

base::Value::Dict MakeSimpleOperationEvent(
    base::expected<base::FilePath, CategorizedError> result,
    const int operation_type) {
  return MakeSimpleOperationEvent(
      result.has_value()
          ? CategorizedError(
                {.category = ErrorCategory::kNone, .code = 0, .extra = 0})
          : result.error(),
      operation_type);
}

base::Value::Dict MakeSimpleOperationEvent(const CategorizedError& error,
                                           const int operation_type) {
  base::Value::Dict event;
  event.Set("eventtype", operation_type);
  event.Set("eventresult",
            static_cast<int>(error.category == ErrorCategory::kNone
                                 ? protocol_request::kEventResultSuccess
                                 : protocol_request::kEventResultError));
  if (error.category != ErrorCategory::kNone) {
    event.Set("errorcat", static_cast<int>(error.category));
  }
  if (error.code != 0) {
    event.Set("errorcode", error.code);
  }
  if (error.extra != 0) {
    event.Set("extracode1", error.extra);
  }
  return event;
}

}  // namespace update_client
