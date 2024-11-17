// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_ANNOTATIONS_USER_ANNOTATIONS_TYPES_H_
#define COMPONENTS_USER_ANNOTATIONS_USER_ANNOTATIONS_TYPES_H_

#include <vector>

#include "base/functional/callback_forward.h"
#include "base/types/expected.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"

namespace optimization_guide::proto {
class UserAnnotationsEntry;
}

namespace autofill {
class FormStructure;
}  // namespace autofill

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

// Encapsulates the result of user interaction with the prediction improvements
// prompt.
struct PromptAcceptanceResult {
  bool prompt_was_accepted = false;
  bool did_user_interact = false;
  bool did_thumbs_up_triggered = false;
  bool did_thumbs_down_triggered = false;
};

// Encapsulates the result of annotating a form submission.
// `to_be_upserted_entries` is the updated entries, that will be shown in the
// Autofill prediction improvements prompt. `model_execution_id` is the server
// log id for model execution, and can be sent in any user submitted feedback.
struct FormAnnotationResponse {
  std::vector<optimization_guide::proto::UserAnnotationsEntry>
      to_be_upserted_entries;
  std::string model_execution_id;

  FormAnnotationResponse(
      const std::vector<optimization_guide::proto::UserAnnotationsEntry>&
          to_be_upserted_entries,
      const std::string& model_execution_id);
  ~FormAnnotationResponse();
};

using PromptAcceptanceCallback =
    base::OnceCallback<void(PromptAcceptanceResult result)>;

// Callback for notifying the form annotation result to Autofill. User decision
// for importing the updated form entries is notified to
// `UserAnnotationsService` via the `prompt_acceptance_callback`.
using ImportFormCallback = base::OnceCallback<void(
    std::unique_ptr<autofill::FormStructure> form,
    std::unique_ptr<FormAnnotationResponse> form_annotation_response,
    PromptAcceptanceCallback prompt_acceptance_callback)>;

}  // namespace user_annotations

#endif  // COMPONENTS_USER_ANNOTATIONS_USER_ANNOTATIONS_TYPES_H_
