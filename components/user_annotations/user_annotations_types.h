// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_ANNOTATIONS_USER_ANNOTATIONS_TYPES_H_
#define COMPONENTS_USER_ANNOTATIONS_USER_ANNOTATIONS_TYPES_H_

#include <vector>

#include "base/types/expected.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"

namespace user_annotations {

typedef int64_t EntryID;

typedef std::vector<optimization_guide::proto::UserAnnotationsEntry>
    UserAnnotationsEntries;

struct Entry {
  // The row ID of this entry from the user annotations database. This is
  // immutable except when retrieving the row from the database.
  EntryID entry_id;

  // The proto for this entry.
  optimization_guide::proto::UserAnnotationsEntry entry_proto;
};

// Encapsulates the result of various operations with user annotations entries.
enum class UserAnnotationsExecutionResult {
  kSuccess = 0,
  kSqlError = 1,
  kCryptNotInitialized = 2,
  kCryptError = 3,
  kResponseError = 4,
  kResponseMalformed = 5,
  kResponseTimedOut = 6,

  // Insert new values before this line. Should be in sync with
  // UserAnnotationsExecutionResult in user_annotations/enums.xml
  kMaxValue = kResponseTimedOut
};

using UserAnnotationsEntryRetrievalResult =
    base::expected<std::vector<optimization_guide::proto::UserAnnotationsEntry>,
                   UserAnnotationsExecutionResult>;

}  // namespace user_annotations

#endif  // COMPONENTS_USER_ANNOTATIONS_USER_ANNOTATIONS_TYPES_H_
