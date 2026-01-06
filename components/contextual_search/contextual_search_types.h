// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_TYPES_H_
#define COMPONENTS_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_TYPES_H_

#include <string>

#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "components/sessions/core/session_id.h"
#include "third_party/lens_server_proto/lens_overlay_request_id.pb.h"
#include "url/gurl.h"
#include "components/lens/contextual_input.h"

namespace lens {
enum class MimeType;
}  // namespace lens

namespace contextual_search {

// Upload status of a file.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.contextual_search
enum class FileUploadStatus {
  // Not uploaded.
  kNotUploaded = 0,
  // File being processed.
  kProcessing = 1,
  // Failed validation - Terminal for this file attempt.
  kValidationFailed = 2,
  // Request sent to Lens server.
  kUploadStarted = 3,
  // Server confirmed successful receipt.
  kUploadSuccessful = 4,
  // Server or network error during upload - Terminal for this file attempt.
  kUploadFailed = 5,
  // File expired.
  kUploadExpired = 6,
  // File being processed, and suggest signals are ready.
  kProcessingSuggestSignalsReady = 7,
};

// For upload error notifications and metrics.
enum class FileUploadErrorType {
  // Unknown.
  kUnknown = 0,
  // Browser error before/during request, not covered by validation.
  kBrowserProcessingError = 1,
  // Network-level issue (e.g., no connectivity, DNS failure).
  kNetworkError = 2,
  // Server returned an error (e.g., 5xx, specific API error).
  kServerError = 3,
  // Server rejected due to size after upload attempt - Considered terminal.
  kServerSizeLimitExceeded = 4,
  // Upload aborted by user deletion or session end.
  kAborted = 5,
  // Image processing error.
  kImageProcessingError = 6,
};

// Struct containing file information for a file upload.
struct FileInfo {
 public:
  FileInfo();
  FileInfo(const FileInfo& other);
  FileInfo& operator=(const FileInfo& other);
  virtual ~FileInfo();

  // Gets the context id for this request.
  int64_t GetContextId() const { return request_id.context_id(); }

  // Client-side unique identifier.
  base::UnguessableToken file_token;

  // Name of the selected file.
  std::string file_name;

  // Size in bytes of the file.
  uint64_t file_size_bytes = 0;

  // The time the file was selected.
  base::Time selection_time;

  // The mime type of the file.
  lens::MimeType mime_type;

  // The upload status of the file.
  // Do not modify this field directly.
  contextual_search::FileUploadStatus upload_status =
      contextual_search::FileUploadStatus::kNotUploaded;

  // The error type if the upload failed.
  // Do not modify this field directly.
  contextual_search::FileUploadErrorType upload_error_type =
      contextual_search::FileUploadErrorType::kUnknown;

  // If populated, the url of the tab corresponding to this uploaded file.
  std::optional<GURL> tab_url;

  // If populated, the title of the tab corresponding to this uploaded file.
  std::optional<std::string> tab_title;

  // If populated, the session id corresponding to the tab.
  std::optional<SessionID> tab_session_id;

  // The request ID for this request. Updated by the context
  // controller when the file upload is started.
  lens::LensOverlayRequestId request_id;

  // The raw response bodies from the upload requests.
  std::vector<std::string> response_bodies;

  // The input data associated with this file.
  std::unique_ptr<lens::ContextualInputData> input_data;
};

// LINT.IfChange(SubmissionType)

// How an AIM Composebox query was submitted.
enum class SubmissionType {
  kDefault = 0,
  kDeepSearch = 1,
  kCreateImages = 2,
  kMaxValue = kCreateImages,
};

// LINT.ThenChange(//tools/metrics/histograms/metadata/contextual_search/enums.xml:SubmissionType)

// LINT.IfChange(AimToolState)

// Value to hold the state of an AIM Tool.
enum class AimToolState {
  kDisabled = 0,
  kEnabled = 1,
  kMaxValue = kEnabled,
};

// LINT.ThenChange(//tools/metrics/histograms/metadata/contextual_search/enums.xml:AimToolState)

// LINT.IfChange(ContextualSearchErrorPage)

// Reasons the contextual search error page appeared.
enum class ContextualSearchErrorPage {
  kUnknown = 0,
  kPageContextNotEligible = 1,
  kMaxValue = kPageContextNotEligible,
};

// LINT.ThenChange(//tools/metrics/histograms/metadata/contextual_search/enums.xml:ContextualSearchErrorPage)

}  // namespace contextual_search

#endif  // COMPONENTS_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_TYPES_H_
