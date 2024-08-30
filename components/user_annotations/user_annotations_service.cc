// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_annotations/user_annotations_service.h"

#include "base/metrics/histogram_macros_local.h"
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
#include "components/user_annotations/user_annotations_database.h"
#include "components/user_annotations/user_annotations_features.h"
#include "components/user_annotations/user_annotations_types.h"

namespace user_annotations {

namespace {

void RecordUserAnnotationsService(bool success) {
  LOCAL_HISTOGRAM_BOOLEAN("UserAnnotations.DidAddFormSubmission", success);
}

}  // namespace

UserAnnotationsService::UserAnnotationsService(
    optimization_guide::OptimizationGuideModelExecutor* model_executor,
    const base::FilePath& storage_dir)
    : model_executor_(model_executor) {
  if (ShouldPersistUserAnnotations()) {
    user_annotations_database_ = base::SequenceBound<UserAnnotationsDatabase>(
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
             base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
        storage_dir);
  }
}

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
    base::OnceCallback<
        void(std::vector<optimization_guide::proto::UserAnnotationsEntry>)>
        callback) {
  if (ShouldPersistUserAnnotations()) {
    user_annotations_database_
        .AsyncCall(&UserAnnotationsDatabase::RetrieveAllEntries)
        .Then(std::move(callback));
    return;
  }

  std::vector<optimization_guide::proto::UserAnnotationsEntry> entries_protos;
  entries_protos.reserve(entries_.size());
  for (const auto& entry : entries_) {
    entries_protos.push_back(entry.entry_proto);
  }
  std::move(callback).Run(std::move(entries_protos));
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
    std::vector<optimization_guide::proto::UserAnnotationsEntry> entries_protos;
    for (const auto& entry : maybe_response->entries()) {
      optimization_guide::proto::UserAnnotationsEntry entry_proto;
      entry_proto.set_key(entry.key());
      entry_proto.set_value(entry.value());
      entries_protos.push_back(std::move(entry_proto));
    }
    user_annotations_database_
        .AsyncCall(&UserAnnotationsDatabase::UpdateEntries)
        .WithArgs(entries_protos)
        .Then(base::BindOnce(RecordUserAnnotationsService));
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
  RecordUserAnnotationsService(true);
}

}  // namespace user_annotations
