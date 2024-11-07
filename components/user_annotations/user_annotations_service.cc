// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_annotations/user_annotations_service.h"

#include "base/callback_list.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_processing/optimization_guide_proto_util.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/geo/phone_number_i18n.h"
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
#include "components/user_annotations/user_annotations_switches.h"
#include "components/user_annotations/user_annotations_types.h"

namespace user_annotations {

namespace {

base::flat_map<autofill::FieldType, std::string>
GetEntryKeyByAutofillFieldType() {
  return {
      {autofill::FieldType::NAME_FIRST, "First Name"},
      {autofill::FieldType::NAME_MIDDLE, "Middle Name"},
      {autofill::FieldType::NAME_LAST, "Last Name"},
      {autofill::FieldType::EMAIL_ADDRESS, "Email Address"},
      {autofill::FieldType::PHONE_HOME_WHOLE_NUMBER, "Phone Number [mobile]"},
      {autofill::FieldType::ADDRESS_HOME_CITY, "Address - City"},
      {autofill::FieldType::ADDRESS_HOME_STATE, "Address - State"},
      {autofill::FieldType::ADDRESS_HOME_ZIP, "Address - Zip Code"},
      {autofill::FieldType::ADDRESS_HOME_COUNTRY, "Address - Country"},
      {autofill::FieldType::ADDRESS_HOME_STREET_ADDRESS, "Address - Street"},
  };
}

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
  std::move(callback).Run(std::move(user_annotations).value());
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

std::string GetEntryValueFromAutofillProfile(
    const autofill::AutofillProfile& autofill_profile,
    autofill::FieldType field_type) {
  if (field_type == autofill::FieldType::PHONE_HOME_WHOLE_NUMBER) {
    return autofill::i18n::FormatPhoneForDisplay(
        base::UTF16ToUTF8(autofill_profile.GetRawInfo(field_type)),
        base::UTF16ToUTF8(
            autofill_profile.GetRawInfo(autofill::PHONE_HOME_COUNTRY_CODE)));
  }
  return base::UTF16ToUTF8(autofill_profile.GetRawInfo(field_type));
}

UserAnnotationsEntries ConvertAutofillProfileToEntries(
    const autofill::AutofillProfile& autofill_profile) {
  static const base::flat_map<autofill::FieldType, std::string>
      entry_key_by_autofill_field_type = GetEntryKeyByAutofillFieldType();
  UserAnnotationsEntries entries;
  for (const auto& [field_type, entry_key] : entry_key_by_autofill_field_type) {
    const std::string entry_value =
        GetEntryValueFromAutofillProfile(autofill_profile, field_type);
    if (entry_value.empty()) {
      continue;
    }
    optimization_guide::proto::UserAnnotationsEntry entry_proto;
    entry_proto.set_key(entry_key);
    entry_proto.set_value(std::move(entry_value));
    entries.emplace_back(std::move(entry_proto));
  }
  return entries;
}

void NotifyAutofillProfileSaved(
    base::OnceCallback<void(UserAnnotationsExecutionResult)> callback,
    UserAnnotationsExecutionResult result) {
  std::move(callback).Run(result);
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
  encryptor_ready_subscription_ = os_crypt_async->GetInstance(
      base::BindOnce(&UserAnnotationsService::OnOsCryptAsyncReady,
                     weak_ptr_factory_.GetWeakPtr(), storage_dir));

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

  // Only allow HTTPS sites.
  if (!url.SchemeIs("https")) {
    return false;
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
  if (!user_annotations_database_) {
    // TODO: b/361696651 - Record the failure.
    return;
  }
  user_annotations_database_
      .AsyncCall(&UserAnnotationsDatabase::RetrieveAllEntries)
      .Then(base::BindOnce(ProcessEntryRetrieval, std::move(callback)));
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

  if (auto manual_entries = switches::ParseFormsAnnotationsFromCommandLine()) {
    RemoveAllEntries(base::BindOnce(
        &UserAnnotationsService::InitializeFormsAnnotationsFromCommandLine,
        weak_ptr_factory_.GetWeakPtr(), *manual_entries));
  }
}

void UserAnnotationsService::InitializeFormsAnnotationsFromCommandLine(
    const optimization_guide::proto::FormsAnnotationsResponse& manual_entries) {
  SaveEntries(manual_entries);
}

void UserAnnotationsService::Shutdown() {}

bool UserAnnotationsService::IsDatabaseReady() {
  return !!user_annotations_database_;
}

void UserAnnotationsService::SaveEntries(
    const optimization_guide::proto::FormsAnnotationsResponse& entries) {
  DCHECK(user_annotations_database_);

  UserAnnotationsEntries upserted_entries = UserAnnotationsEntries(
      entries.upserted_entries().begin(), entries.upserted_entries().end());
  std::set<EntryID> deleted_entry_ids(entries.deleted_entry_ids().begin(),
                                      entries.deleted_entry_ids().end());
  user_annotations_database_.AsyncCall(&UserAnnotationsDatabase::UpdateEntries)
      .WithArgs(upserted_entries, deleted_entry_ids)
      .Then(base::BindOnce(RecordUserAnnotationsFormImportResult));
}

void UserAnnotationsService::SaveAutofillProfile(
    const autofill::AutofillProfile& autofill_profile,
    base::OnceCallback<void(UserAnnotationsExecutionResult)> callback) {
  const UserAnnotationsEntries entries =
      ConvertAutofillProfileToEntries(autofill_profile);
  DCHECK(user_annotations_database_);

  user_annotations_database_.AsyncCall(&UserAnnotationsDatabase::UpdateEntries)
      .WithArgs(entries, std::set<EntryID>{})
      .Then(base::BindOnce(NotifyAutofillProfileSaved, std::move(callback)));
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
