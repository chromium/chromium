// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_annotations/user_annotations_service.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/autofill/core/common/form_data.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/optimization_guide/proto/features/forms_annotations.pb.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/user_annotations/user_annotations_database.h"
#include "components/user_annotations/user_annotations_features.h"
#include "components/user_annotations/user_annotations_types.h"

namespace user_annotations {

namespace {

void RecordUserAnnotationsFormSubmissionResult(
    UserAnnotationsExecutionResult result) {
  base::UmaHistogramEnumeration("UserAnnotations.AddFormSubmissionResult",
                                result);
}

void ProcessEntryRetrieval(
    base::OnceCallback<void(UserAnnotationsEntries)> callback,
    UserAnnotationsEntryRetrievalResult user_annotations) {
  // TODO: b/36169665 - Record the entry retrieval result metrics.
  if (!user_annotations.has_value()) {
    std::move(callback).Run({});
    return;
  }
  std::move(callback).Run(user_annotations.value());
}

}  // namespace

UserAnnotationsService::UserAnnotationsService(
    optimization_guide::OptimizationGuideModelExecutor* model_executor,
    const base::FilePath& storage_dir,
    os_crypt_async::OSCryptAsync* os_crypt_async)
    : model_executor_(model_executor) {
  if (ShouldPersistUserAnnotations()) {
    encryptor_ready_subscription_ = os_crypt_async->GetInstance(
        base::BindOnce(&UserAnnotationsService::OnOsCryptAsyncReady,
                       weak_ptr_factory_.GetWeakPtr(), storage_dir));
  }
}

UserAnnotationsService::UserAnnotationsService() = default;

UserAnnotationsService::~UserAnnotationsService() = default;

void UserAnnotationsService::AddFormSubmission(
    optimization_guide::proto::AXTreeUpdate ax_tree_update,
    const autofill::FormData& form_data) {
  // Construct request.
  optimization_guide::proto::FormsAnnotationsRequest request;
  optimization_guide::proto::PageContext* page_context =
      request.mutable_page_context();
  page_context->set_url(form_data.url().spec());
  page_context->set_title(ax_tree_update.tree_data().title());
  *page_context->mutable_ax_tree_data() = std::move(ax_tree_update);
  *request.mutable_form_data() = optimization_guide::ToFormDataProto(form_data);
  for (const auto& entry : entries_) {
    *request.add_entries() = entry.entry_proto;
  }

  model_executor_->ExecuteModel(
      optimization_guide::ModelBasedCapabilityKey::kFormsAnnotations, request,
      base::BindOnce(&UserAnnotationsService::OnModelExecuted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void UserAnnotationsService::RetrieveAllEntries(
    base::OnceCallback<void(UserAnnotationsEntries)> callback) {
  if (ShouldPersistUserAnnotations()) {
    if (!user_annotations_database_) {
      // TODO: b/361696651 - Record the failure.
      return;
    }
    user_annotations_database_
        .AsyncCall(&UserAnnotationsDatabase::RetrieveAllEntries)
        .Then(base::BindOnce(ProcessEntryRetrieval, std::move(callback)));
    return;
  }

  UserAnnotationsEntries entries_protos;
  entries_protos.reserve(entries_.size());
  for (const auto& entry : entries_) {
    entries_protos.push_back(entry.entry_proto);
  }
  std::move(callback).Run(std::move(entries_protos));
}

void UserAnnotationsService::OnOsCryptAsyncReady(
    const base::FilePath& storage_dir,
    os_crypt_async::Encryptor encryptor,
    bool success) {
  if (!success) {
    // TODO: b/361696651 - Record the failure.
    return;
  }
  user_annotations_database_ = base::SequenceBound<UserAnnotationsDatabase>(
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
      storage_dir, std::move(encryptor));
}

void UserAnnotationsService::Shutdown() {}

void UserAnnotationsService::OnModelExecuted(
    optimization_guide::OptimizationGuideModelExecutionResult result,
    std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry) {
  if (!result.has_value()) {
    return;
  }

  std::optional<optimization_guide::proto::FormsAnnotationsResponse>
      maybe_response = optimization_guide::ParsedAnyMetadata<
          optimization_guide::proto::FormsAnnotationsResponse>(result.value());
  if (!maybe_response) {
    return;
  }

  if (ShouldPersistUserAnnotations()) {
    if (!user_annotations_database_) {
      RecordUserAnnotationsFormSubmissionResult(
          UserAnnotationsExecutionResult::kCryptNotInitialized);
      return;
    }

    UserAnnotationsEntries entries_protos;
    for (const auto& entry : maybe_response->entries()) {
      optimization_guide::proto::UserAnnotationsEntry entry_proto;
      entry_proto.set_key(entry.key());
      entry_proto.set_value(entry.value());
      entries_protos.push_back(std::move(entry_proto));
    }
    user_annotations_database_
        .AsyncCall(&UserAnnotationsDatabase::UpdateEntries)
        .WithArgs(entries_protos)
        .Then(base::BindOnce(RecordUserAnnotationsFormSubmissionResult));
    return;
  }

  if (ShouldReplaceAnnotationsAfterEachSubmission()) {
    entries_.clear();
  }

  for (const auto& entry : maybe_response->entries()) {
    optimization_guide::proto::UserAnnotationsEntry entry_proto;
    entry_proto.set_key(entry.key());
    entry_proto.set_value(entry.value());
    entries_.push_back({.entry_id = ++entry_id_counter_,
                        .entry_proto = std::move(entry_proto)});
  }
  RecordUserAnnotationsFormSubmissionResult(
      UserAnnotationsExecutionResult::kSuccess);
}

}  // namespace user_annotations
