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
#include "base/types/expected.h"
#include "components/autofill/core/browser/form_processing/optimization_guide_proto_util.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/optimization_guide/core/optimization_guide_decider.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/optimization_guide/proto/features/forms_annotations.pb.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/user_annotations/form_submission_handler.h"
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

void RecordCountEntriesResult(UserAnnotationsExecutionResult result) {
  base::UmaHistogramEnumeration("UserAnnotations.CountEntries.Result", result);
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
    const GURL& url,
    const std::string& title,
    optimization_guide::proto::AXTreeUpdate ax_tree_update,
    std::unique_ptr<autofill::FormStructure> form,
    ImportFormCallback callback) {
  // `form` is assumed to never be `nullptr`.
  CHECK(form);
  pending_form_submissions_.emplace(std::make_unique<FormSubmissionHandler>(
      this, url, title, std::move(ax_tree_update), std::move(form),
      std::move(callback)));
  if (pending_form_submissions_.size() != 1) {
    return;
  }
  ProcessNextFormSubmission();
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

bool UserAnnotationsService::IsDatabaseReady() {
  return !!user_annotations_database_;
}

void UserAnnotationsService::SaveEntries(
    const optimization_guide::proto::FormsAnnotationsResponse& entries) {
  if (ShouldPersistUserAnnotations()) {
    DCHECK(user_annotations_database_);

    UserAnnotationsEntries upserted_entries = UserAnnotationsEntries(
        entries.upserted_entries().begin(), entries.upserted_entries().end());
    std::set<EntryID> deleted_entry_ids(entries.deleted_entry_ids().begin(),
                                        entries.deleted_entry_ids().end());
    user_annotations_database_
        .AsyncCall(&UserAnnotationsDatabase::UpdateEntries)
        .WithArgs(upserted_entries, deleted_entry_ids)
        .Then(base::BindOnce(RecordUserAnnotationsFormImportResult));
    return;
  }

  for (const auto& entry : entries.upserted_entries()) {
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

void UserAnnotationsService::OnFormSubmissionComplete() {
  pending_form_submissions_.pop();
  ProcessNextFormSubmission();
}

void UserAnnotationsService::ProcessNextFormSubmission() {
  if (pending_form_submissions_.empty()) {
    return;
  }
  pending_form_submissions_.front()->Start();
}

void UserAnnotationsService::RemoveEntry(EntryID entry_id,
                                         base::OnceClosure callback) {
  if (!ShouldPersistUserAnnotations()) {
    std::erase_if(entries_, [entry_id](const Entry& entry) {
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

void UserAnnotationsService::GetCountOfValuesContainedBetween(
    base::Time begin,
    base::Time end,
    base::OnceCallback<void(int)> callback) {
  if (!ShouldPersistUserAnnotations()) {
    RecordCountEntriesResult(UserAnnotationsExecutionResult::kSuccess);
    // This code path will get removed soon but given no annotations are removed
    // when a specific range is selected in this code path, also do not indicate
    // we are removing entries here.
    std::move(callback).Run(0);
    return;
  }
  if (!user_annotations_database_) {
    RecordCountEntriesResult(
        UserAnnotationsExecutionResult::kCryptNotInitialized);
    std::move(callback).Run(0);
    return;
  }
  user_annotations_database_
      .AsyncCall(&UserAnnotationsDatabase::GetCountOfValuesContainedBetween)
      .WithArgs(begin, end)
      .Then(base::BindOnce(
          [](base::OnceCallback<void(int)> callback, int result) {
            RecordCountEntriesResult(
                result ? UserAnnotationsExecutionResult::kSuccess
                       : UserAnnotationsExecutionResult::kSqlError);
            std::move(callback).Run(result);
          },
          std::move(callback)));
}

}  // namespace user_annotations
