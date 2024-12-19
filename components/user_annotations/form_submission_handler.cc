// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_annotations/form_submission_handler.h"

#include "base/metrics/histogram_functions.h"
#include "components/autofill/core/browser/form_processing/optimization_guide_proto_util.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/optimization_guide/core/model_quality/feature_type_map.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/forms_annotations.pb.h"
#include "components/user_annotations/user_annotations_features.h"
#include "components/user_annotations/user_annotations_service.h"

namespace user_annotations {

FormSubmissionHandler::FormSubmissionHandler(
    UserAnnotationsService* user_annotations_service,
    const GURL& url,
    const std::string& title,
    optimization_guide::proto::AXTreeUpdate ax_tree_update,
    std::unique_ptr<autofill::FormStructure> form,
    ImportFormCallback callback)
    : url_(url),
      title_(title),
      ax_tree_update_(std::move(ax_tree_update)),
      form_(std::move(form)),
      callback_(std::move(callback)),
      user_annotations_service_(user_annotations_service) {}

FormSubmissionHandler::~FormSubmissionHandler() = default;

void FormSubmissionHandler::Start() {
  completion_timer_.Start(
      FROM_HERE, GetFormSubmissionCompletionTimeout(),
      base::BindOnce(&FormSubmissionHandler::OnCompletionTimeout,
                     weak_ptr_factory_.GetWeakPtr()));
  user_annotations_service_->RetrieveAllEntries(
      base::BindOnce(&FormSubmissionHandler::ExecuteModelWithEntries,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FormSubmissionHandler::OnCompletionTimeout() {
  // TODO: 372715386 - Handle the timeout and notify the save prompt UX.
}

void FormSubmissionHandler::ExecuteModelWithEntries(
    UserAnnotationsEntries entries) {
  // Construct request.
  optimization_guide::proto::FormsAnnotationsRequest request;
  optimization_guide::proto::PageContext* page_context =
      request.mutable_page_context();
  page_context->set_url(url_.spec());
  page_context->set_title(title_);
  *page_context->mutable_ax_tree_data() = std::move(ax_tree_update_);
  *request.mutable_form_data() = autofill::ToFormDataProto(*form_);
  *request.mutable_entries() = {std::make_move_iterator(entries.begin()),
                                std::make_move_iterator(entries.end())};
  user_annotations_service_->model_executor()->ExecuteModel(
      optimization_guide::ModelBasedCapabilityKey::kFormsAnnotations, request,
      /*execution_timeout=*/std::nullopt,
      base::BindOnce(&FormSubmissionHandler::OnModelExecuted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FormSubmissionHandler::OnModelExecuted(
    optimization_guide::OptimizationGuideModelExecutionResult result,
    std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry) {
  if (!result.response.has_value()) {
    SendFormSubmissionResult(
        base::unexpected(UserAnnotationsExecutionResult::kResponseError),
        std::move(log_entry));
    return;
  }

  std::optional<optimization_guide::proto::FormsAnnotationsResponse>
      maybe_response = optimization_guide::ParsedAnyMetadata<
          optimization_guide::proto::FormsAnnotationsResponse>(
          result.response.value());
  if (!maybe_response) {
    SendFormSubmissionResult(
        base::unexpected(UserAnnotationsExecutionResult::kResponseMalformed),
        std::move(log_entry));
    return;
  }

  if (!user_annotations_service_->IsDatabaseReady()) {
    SendFormSubmissionResult(
        base::unexpected(UserAnnotationsExecutionResult::kCryptNotInitialized),
        std::move(log_entry));
    return;
  }

  SendFormSubmissionResult(base::ok(maybe_response.value()),
                           std::move(log_entry));
}

void FormSubmissionHandler::SendFormSubmissionResult(
    FormSubmissionResult result,
    std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry) {
  base::UmaHistogramEnumeration(
      "UserAnnotations.AddFormSubmissionResult",
      result.error_or(UserAnnotationsExecutionResult::kSuccess));
  if (!result.has_value()) {
    CHECK_NE(result.error(), UserAnnotationsExecutionResult::kSuccess);
    optimization_guide::ModelQualityLogEntry::Drop(std::move(log_entry));
    if (callback_) {
      std::move(callback_).Run(
          std::move(form_),
          /*form_annotation_response=*/nullptr,
          /*prompt_acceptance_callback=*/base::DoNothing());
    }
    NotifyCompletion();
    return;
  }

  if (result->upserted_entries().empty()) {
    optimization_guide::ModelQualityLogEntry::Drop(std::move(log_entry));
    std::move(callback_).Run(std::move(form_),
                             /*form_annotation_response=*/nullptr,
                             /*prompt_acceptance_callback=*/base::DoNothing());
    NotifyCompletion();
    return;
  }
  auto form_annotation_response = std::make_unique<FormAnnotationResponse>(
      UserAnnotationsEntries(result->upserted_entries().begin(),
                             result->upserted_entries().end()),
      log_entry->model_execution_id());
  std::move(callback_).Run(
      std::move(form_), std::move(form_annotation_response),
      base::BindOnce(&FormSubmissionHandler::OnImportFormConfirmation,
                     weak_ptr_factory_.GetWeakPtr(), std::move(result),
                     std::move(log_entry)));
}

void FormSubmissionHandler::OnImportFormConfirmation(
    FormSubmissionResult result,
    std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry,
    PromptAcceptanceResult prompt_acceptance_result) {
  if (!prompt_acceptance_result.did_user_interact &&
      !prompt_acceptance_result.did_thumbs_down_triggered &&
      !prompt_acceptance_result.did_thumbs_up_triggered) {
    // Drop the log entry, when the user did no interaction with the save prompt
    // bubble.
    optimization_guide::ModelQualityLogEntry::Drop(std::move(log_entry));
    NotifyCompletion();
    return;
  }
  if (log_entry) {
    auto* quality_entry = log_entry->quality_data<
        optimization_guide::FormsAnnotationsFeatureTypeMap>();
    if (prompt_acceptance_result.did_thumbs_down_triggered) {
      quality_entry->set_user_feedback(
          optimization_guide::proto::UserFeedback::USER_FEEDBACK_THUMBS_DOWN);
    } else if (prompt_acceptance_result.did_thumbs_up_triggered) {
      quality_entry->set_user_feedback(
          optimization_guide::proto::UserFeedback::USER_FEEDBACK_THUMBS_UP);
    }
    quality_entry->set_save_prompt_action(
        prompt_acceptance_result.prompt_was_accepted
            ? optimization_guide::proto::FormsAnnotationsSavePromptAction::
                  FORMS_ANNOTATIONS_SAVE_PROMPT_ACTION_ACCEPTED
            : optimization_guide::proto::FormsAnnotationsSavePromptAction::
                  FORMS_ANNOTATIONS_SAVE_PROMPT_ACTION_REJECTED);
    optimization_guide::ModelQualityLogEntry::Upload(std::move(log_entry));
  }

  if (!prompt_acceptance_result.prompt_was_accepted) {
    NotifyCompletion();
    return;
  }
  user_annotations_service_->SaveEntries(result.value());
  NotifyCompletion();
}

void FormSubmissionHandler::NotifyCompletion() {
  completion_timer_.Stop();
  user_annotations_service_->OnFormSubmissionComplete();
}

}  // namespace user_annotations
