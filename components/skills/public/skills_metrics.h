// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SKILLS_PUBLIC_SKILLS_METRICS_H_
#define COMPONENTS_SKILLS_PUBLIC_SKILLS_METRICS_H_

#include <cstddef>
namespace skills {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.

// LINT.IfChange(SkillsDialogAction)
enum class SkillsDialogAction {
  kOpened = 0,
  kSaved = 1,
  kCancelled = 2,
  kRefined = 3,
  kMaxValue = kRefined,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/skills/enums.xml:SkillsDialogAction)

// LINT.IfChange(SkillsFetchResult)
enum class SkillsFetchResult {
  kSuccess = 0,
  kEmptyResponseBody = 1,
  kEmptyResponseHeader = 2,
  kProtoParseFailure = 3,
  kNetworkError = 4,
  kMaxValue = kNetworkError,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/skills/enums.xml:SkillsFetchResult)

// LINT.IfChange(SkillsInvokeAction)
enum class SkillsInvokeAction {
  kFirstParty = 0,
  kUserCreated = 1,
  kDerivedFromFirstParty = 2,
  kMaxValue = kDerivedFromFirstParty,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/skills/enums.xml:SkillsInvokeAction)

// LINT.IfChange(SkillsInvokeResult)
enum class SkillsInvokeResult {
  kSuccess = 0,
  // Tried to execute a nonexistent or deleted skill ID
  kSkillNotFound = 1,
  // The Glic panel took too long to open
  kTimeout = 2,
  kMaxValue = kTimeout,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/skills/enums.xml:SkillsInvokeResult)

// LINT.IfChange(SkillsSaveResult)
enum class SkillsSaveResult {
  kSuccess = 0,
  // The UI was closed before the operation finished
  kUiContextLost = 1,
  // Tried to update a nonexistent or deleted skill ID
  kSkillNotFound = 2,
  // The underlying database or Sync layer failed to write the new data
  kWriteFailed = 3,
  // SkillsService was not found
  kServiceNotFound = 4,
  // Sync hasn't finished initializing on startup
  kServiceNotReady = 5,
  kMaxValue = kServiceNotReady,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/skills/enums.xml:SkillsSaveResult)

// LINT.IfChange(SkillsRefineResult)
enum class SkillsRefineResult {
  kSuccess = 0,
  // Model failed to return a response
  kModelExecutionFailed = 1,
  // Failed to parse the protobuf
  kParseError = 2,
  // Model returned a response, but the list was empty
  kNoSuggestions = 3,
  // OptimizationGuideKeyedService was null
  kServiceUnavailable = 4,
  // UI sent an empty prompt
  kInvalidRequest = 5,
  kMaxValue = kInvalidRequest,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/skills/enums.xml:SkillsRefineResult)
// LINT.IfChange(SkillsDownloadRequestStatus)
enum class SkillsDownloadRequestStatus {
  kSent = 0,
  kResponseReceived = 1,
  kAlreadyRunning = 2,
  kTimedOut = 3,
  kMaxValue = kTimedOut,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/skills/enums.xml:SkillsDownloadRequestStatus)

// LINT.IfChange(SkillsManagementError)
enum class SkillsManagementError {
  kTabControllerDNE = 0,
  kSkillsServiceNotReady = 1,
  k1pSkillDNE = 2,
  kMaxValue = k1pSkillDNE,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/skills/enums.xml:SkillsManagementError)

// TODO(crbug.com/477385216): Update to use an enum for creation mode.
// Records user interactions within the Skills Creation or Edit dialogs
void RecordSkillsDialogAction(SkillsDialogAction action, bool is_edit_mode);

// Records the execution of a skill and its type.
void RecordSkillsInvokeAction(SkillsInvokeAction action);

// Records the terminal outcome (success or specific error) of attempting to
// invoke a skill. This is logged after the client attempts to open the panel
// and trigger the skill execution.
void RecordSkillsInvokeResult(SkillsInvokeResult result);

// Records the terminal outcome of an attempt to save a new skill or update
// an existing one. This captures backend availability, database write status,
// and UI context state.
void RecordSkillsSaveResult(SkillsSaveResult result);

// Records the terminal outcome of a skill prompt refinement request. This
// captures the success or failure of the Optimization Guide ML model execution
// and response parsing.
void RecordSkillsRefineResult(SkillsRefineResult result);

// Records the current total number of skills the user possesses.
// This is called periodically by the SkillsMetricsProvider to capture
// the user's status throughout the session.
void RecordUserSkillCount(size_t skill_count);

// Records the result of a first-party skill list download attempt from static
// content server link.
void RecordSkillsFetchResult(SkillsFetchResult result);

// Records the HTTP response code received when downloading skills.
void RecordSkillsHttpCode(int http_code);

// Records the status of a skills download request.
void RecordSkillsDownloadRequestStatus(SkillsDownloadRequestStatus status);

// Records errors encountered during skills management operations.
void RecordSkillsManagementError(SkillsManagementError error);

}  // namespace skills

#endif  // COMPONENTS_SKILLS_PUBLIC_SKILLS_METRICS_H_
