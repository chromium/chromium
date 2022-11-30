// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/execution_status.h"

namespace optimization_guide {

std::string ExecutionStatusToString(ExecutionStatus status) {
  switch (status) {
    case ExecutionStatus::kUnknown:
      return "Unknown";
    case ExecutionStatus::kSuccess:
      return "Success";
    case ExecutionStatus::kPending:
      return "Pending";
    case ExecutionStatus::kErrorModelFileNotAvailable:
      return "ErrorModelFileNotAvailable";
    case ExecutionStatus::kErrorModelFileNotValid:
      return "ErrorModelFileNotValid";
    case ExecutionStatus::kErrorEmptyOrInvalidInput:
      return "ErrorEmptyOrInvalidInput";
    case ExecutionStatus::kErrorUnknown:
      return "ErrorUnknown";
    case ExecutionStatus::kErrorCancelled:
      return "ErrorCancelled";
  }
}

}  // namespace optimization_guide