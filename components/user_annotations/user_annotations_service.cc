// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_annotations/user_annotations_service.h"

#include "base/callback_list.h"
#include "base/containers/contains.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/autofill/core/common/form_data.h"
#include "components/optimization_guide/core/optimization_guide_decider.h"
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

void RecordUserAnnotationsFormImportResult(
    UserAnnotationsExecutionResult result) {
  base::UmaHistogramEnumeration("UserAnnotations.FormImportResult", result);
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

void RecordRemoveEntryResult(UserAnnotationsExecutionResult result) {
  base::UmaHistogramEnumeration("UserAnnotations.RemoveEntry.Result", result);
}

void RecordRemoveAllEntriesResult(UserAnnotationsExecutionResult result) {
  base::UmaHistogramEnumeration("UserAnnotations.RemoveAllEntries.Result",
                                result);
}

}  // namespace

UserAnnotationsService::UserAnnotationsService(
    optimization_guide::OptimizationGuideModelExecutor* model_executor,
    const base::FilePath& storage_dir,
    os_crypt_async::OSCryptAsync* os_crypt_async,
    optimization_guide::OptimizationGuideDecider* optimization_guide_decider)
    : model_executor_(model_executor),
      optimization_guide_decider_(optimization_guide_decider),
      allowed_hosts_for_forms_annotations_(
          GetAllowedHostsForFormsAnnotations()) {
  if (ShouldPersistUserAnnotations()) {
    encryptor_ready_subscription_ = os_crypt_async->GetInstance(
        base::BindOnce(&UserAnnotationsService::OnOsCryptAsyncReady,
                       weak_ptr_factory_.GetWeakPtr(), storage_dir));
  }

  if (optimization_guide_decider_) {
    optimization_guide_decider_->RegisterOptimizationTypes(
        {optimization_guide::proto::FORMS_ANNOTATIONS});
  }
}

UserAnnotationsService::UserAnnotationsService() = default;

UserAnnotationsService::~UserAnnotationsService() = default;

bool UserAnnotationsService::ShouldAddFormSubmissionForURL(const GURL& url) {
  if (base::Contains(allowed_hosts_for_forms_annotations_, url.host())) {
    return true;
  }

  // Fall back to optimization guide if not in override list.
  if (optimization_guide_decider_) {
    optimization_guide::OptimizationGuideDecision decision =
        optimization_guide_decider_->CanApplyOptimization(
            url, optimization_guide::proto::FORMS_ANNOTATIONS,
            /*metadata=*/nullptr);
    return decision == optimization_guide::OptimizationGuideDecision::kTrue;
  }

  return false;
}

void UserAnnotationsService::AddFormSubmission(
    optimization_guide::proto::AXTreeUpdate ax_tree_update,
    const autofill::FormData& form_data,
    ImportFormCallback callback) {
  // Construct request.
  optimization_guide::proto::FormsAnnotationsRequest request;
  optimization_guide::proto::PageContext* page_context =
      request.mutable_page_context();
  page_context->set_url(form_data.url().spec());
  page_context->set_title(ax_tree_update.tree_data().title());
  *page_context->mutable_ax_tree_data() = std::move(ax_tree_update);
  *request.mutable_form_data() = optimization_guide::ToFormDataProto(form_data);
  RetrieveAllEntries(base::BindOnce(
      &UserAnnotationsService::ExecuteModelWithEntries,
      weak_ptr_factory_.GetWeakPtr(), request, std::move(callback)));
}

void UserAnnotationsService::ExecuteModelWithEntries(
    optimization_guide::proto::FormsAnnotationsRequest request,
    ImportFormCallback callback,
    UserAnnotationsEntries entries) {
  for (const auto& entry : entries) {
    *request.add_entries() = entry;
  }
  model_executor_->ExecuteModel(
      optimization_guide::ModelBasedCapabilityKey::kFormsAnnotations, request,
      base::BindOnce(&UserAnnotationsService::OnModelExecuted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
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
    ImportFormCallback callback,
    optimization_guide::OptimizationGuideModelExecutionResult result,
    std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry) {
  if (!result.has_value()) {
    SendFormSubmissionResult(std::move(callback), /*to_be_upserted_entries=*/{},
                             UserAnnotationsExecutionResult::kResponseError);
    return;
  }

  std::optional<optimization_guide::proto::FormsAnnotationsResponse>
      maybe_response = optimization_guide::ParsedAnyMetadata<
          optimization_guide::proto::FormsAnnotationsResponse>(result.value());
  if (!maybe_response) {
    SendFormSubmissionResult(
        std::move(callback), /*to_be_upserted_entries=*/{},
        UserAnnotationsExecutionResult::kResponseMalformed);
    return;
  }

  if (ShouldPersistUserAnnotations() && !user_annotations_database_) {
    SendFormSubmissionResult(
        std::move(callback), /*to_be_upserted_entries=*/{},
        UserAnnotationsExecutionResult::kCryptNotInitialized);
    return;
  }

  SendFormSubmissionResult(
      std::move(callback),
      /*to_be_upserted_entries=*/
      UserAnnotationsEntries(maybe_response->added_entries().begin(),
                             maybe_response->added_entries().end()),
      UserAnnotationsExecutionResult::kSuccess);
}

void UserAnnotationsService::OnImportFormConfirmation(
    const UserAnnotationsEntries& entries,
    bool prompt_was_accepted) {
  if (!prompt_was_accepted) {
    return;
  }
  if (ShouldPersistUserAnnotations()) {
    DCHECK(user_annotations_database_);

    // TODO: b/366278416 - The database should support inserting, updating,
    // deleting the entries correctly. Currently, it only inserts the entries.
    user_annotations_database_
        .AsyncCall(&UserAnnotationsDatabase::UpdateEntries)
        .WithArgs(entries)
        .Then(base::BindOnce(RecordUserAnnotationsFormImportResult));
    return;
  }

  if (ShouldReplaceAnnotationsAfterEachSubmission()) {
    entries_.clear();
  }

  for (const auto& entry : entries) {
    EntryID entry_id = ++entry_id_counter_;
    optimization_guide::proto::UserAnnotationsEntry entry_proto;
    entry_proto.set_entry_id(entry_id);
    entry_proto.set_key(entry.key());
    entry_proto.set_value(entry.value());
    entries_.push_back(
        {.entry_id = entry_id, .entry_proto = std::move(entry_proto)});
  }
  RecordUserAnnotationsFormImportResult(
      UserAnnotationsExecutionResult::kSuccess);
}

void UserAnnotationsService::SendFormSubmissionResult(
    UserAnnotationsService::ImportFormCallback callback,
    const UserAnnotationsEntries& to_be_upserted_entries,
    UserAnnotationsExecutionResult result) {
  base::UmaHistogramEnumeration("UserAnnotations.AddFormSubmissionResult",
                                result);
  if (result != UserAnnotationsExecutionResult::kSuccess) {
    std::move(callback).Run(
        /*to_be_upserted_entries=*/{},
        /*prompt_acceptance_callback=*/base::DoNothing());
    return;
  }
  // TODO(crbug.com/366142497): Bind to `prompt_acceptance_callback` and only
  // import any form data if the binding method's parameter
  // `prompt_was_accepted` equals `true`.
  std::move(callback).Run(
      to_be_upserted_entries,
      base::BindOnce(&UserAnnotationsService::OnImportFormConfirmation,
                     weak_ptr_factory_.GetWeakPtr(), to_be_upserted_entries));
}

void UserAnnotationsService::RemoveEntry(EntryID entry_id,
                                         base::OnceClosure callback) {
  if (!ShouldPersistUserAnnotations()) {
    std::erase_if(entries_, [entry_id](Entry entry) {
      return entry.entry_id == entry_id;
    });
    RecordRemoveEntryResult(UserAnnotationsExecutionResult::kSuccess);
    std::move(callback).Run();
    return;
  }
  if (!user_annotations_database_) {
    RecordRemoveEntryResult(
        UserAnnotationsExecutionResult::kCryptNotInitialized);
    std::move(callback).Run();
    return;
  }
  user_annotations_database_.AsyncCall(&UserAnnotationsDatabase::RemoveEntry)
      .WithArgs(entry_id)
      .Then(base::BindOnce(
          [](base::OnceClosure callback, bool result) {
            RecordRemoveEntryResult(
                result ? UserAnnotationsExecutionResult::kSuccess
                       : UserAnnotationsExecutionResult::kSqlError);
            std::move(callback).Run();
          },
          std::move(callback)));
}

void UserAnnotationsService::RemoveAllEntries(base::OnceClosure callback) {
  if (!ShouldPersistUserAnnotations()) {
    entries_.clear();
    RecordRemoveAllEntriesResult(UserAnnotationsExecutionResult::kSuccess);
    std::move(callback).Run();
    return;
  }
  if (!user_annotations_database_) {
    RecordRemoveAllEntriesResult(
        UserAnnotationsExecutionResult::kCryptNotInitialized);
    std::move(callback).Run();
    return;
  }
  user_annotations_database_
      .AsyncCall(&UserAnnotationsDatabase::RemoveAllEntries)
      .Then(base::BindOnce(
          [](base::OnceClosure callback, bool result) {
            RecordRemoveAllEntriesResult(
                result ? UserAnnotationsExecutionResult::kSuccess
                       : UserAnnotationsExecutionResult::kSqlError);
            std::move(callback).Run();
          },
          std::move(callback)));
}

void UserAnnotationsService::RemoveAnnotationsInRange(
    const base::Time& delete_begin,
    const base::Time& delete_end) {
  if (!user_annotations_database_) {
    return;
  }
  user_annotations_database_
      .AsyncCall(&UserAnnotationsDatabase::RemoveAnnotationsInRange)
      .WithArgs(delete_begin, delete_end);
}

}  // namespace user_annotations
