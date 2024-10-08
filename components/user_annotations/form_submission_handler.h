// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_ANNOTATIONS_FORM_SUBMISSION_HANDLER_H_
#define COMPONENTS_USER_ANNOTATIONS_FORM_SUBMISSION_HANDLER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "components/user_annotations/user_annotations_service.h"

namespace autofill {
class FormStructure;
}  // namespace autofill

namespace optimization_guide {
class OptimizationGuideDecider;
namespace proto {
class AXTreeUpdate;
}  // namespace proto
}  // namespace optimization_guide

namespace user_annotations {

// Handles the various stages of one form submission. First, the existing
// annotations are retrieved from database, and sent along with the form entries
// to the form annotations model execution. Then the returned annotations are
// sent for user confirmation, before they are persisted to the database.
class FormSubmissionHandler {
 public:
  FormSubmissionHandler(UserAnnotationsService* user_annotations_service,
                        const GURL& url,
                        const std::string& title,
                        optimization_guide::proto::AXTreeUpdate ax_tree_update,
                        std::unique_ptr<autofill::FormStructure> form,
                        UserAnnotationsService::ImportFormCallback callback);
  ~FormSubmissionHandler();

  FormSubmissionHandler(const FormSubmissionHandler&) = delete;
  FormSubmissionHandler& operator=(const FormSubmissionHandler&) = delete;

  // Starts the form submission process.
  void Start();

 private:
  using FormSubmissionResult =
      base::expected<optimization_guide::proto::FormsAnnotationsResponse,
                     UserAnnotationsExecutionResult>;

  void ExecuteModelWithEntries(UserAnnotationsEntries entries);

  // Processes model execution response. Invoked when model execution has been
  // received.
  void OnModelExecuted(
      optimization_guide::OptimizationGuideModelExecutionResult result,
      std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry);

  // Sends the result of form submission.
  void SendFormSubmissionResult(
      FormSubmissionResult result,
      std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry);

  // Called when decision has been made whether to import form entries.
  // `prompt_was_accepted` is the user decision, and `entries` will be
  // persisted to database when true.
  void OnImportFormConfirmation(
      FormSubmissionResult result,
      std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry,
      bool prompt_was_accepted);

  // Called when the timeout is triggered.
  void OnCompletionTimeout();

  // Notifies the service that the form submission stages have been complete.
  void NotifyCompletion();

  GURL url_;
  std::string title_;
  optimization_guide::proto::AXTreeUpdate ax_tree_update_;
  std::unique_ptr<autofill::FormStructure> form_;
  UserAnnotationsService::ImportFormCallback callback_;

  // Guaranteed to outlive `this`.
  raw_ptr<UserAnnotationsService> user_annotations_service_;

  // Timer to enforce timeout on the form submission completion.
  base::OneShotTimer completion_timer_;

  base::WeakPtrFactory<FormSubmissionHandler> weak_ptr_factory_{this};
};

}  // namespace user_annotations

#endif  // COMPONENTS_USER_ANNOTATIONS_FORM_SUBMISSION_HANDLER_H_
