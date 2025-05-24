// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_PIPELINE_UTIL_H_
#define COMPONENTS_UPDATE_CLIENT_PIPELINE_UTIL_H_

#include "base/types/expected.h"
#include "base/values.h"
#include "components/update_client/update_client_errors.h"

namespace base {
class FilePath;
}

namespace update_client {

// Convenience function to make a simple event for an operation
// from the error contained by a base::expected, if one exists.
base::Value::Dict MakeSimpleOperationEvent(
    base::expected<base::FilePath, CategorizedError> result,
    const int operation_type);

// Make a simple event for an operation from a CategorizedError.
base::Value::Dict MakeSimpleOperationEvent(const CategorizedError& error,
                                           const int operation_type);

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_PIPELINE_UTIL_H_
