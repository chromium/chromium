// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ml_model/autofill_ai/autofill_ai_model_cache_impl.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/to_vector.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "components/autofill/core/browser/data_model/data_model_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/proto/server.pb.h"
#include "components/autofill/core/common/signatures.h"
#include "components/history/core/browser/history_service.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/leveldb_proto/public/proto_database_provider.h"

namespace autofill {

namespace {

// A helper for serializing time used in this DB.
constexpr int64_t SerializeTime(base::Time time) {
  return time.ToDeltaSinceWindowsEpoch().InMicroseconds();
}

}  // namespace

AutofillAiModelCacheImpl::AutofillAiModelCacheImpl(
    history::HistoryService* history_service,
    leveldb_proto::ProtoDatabaseProvider* db_provider,
    const base::FilePath& profile_path,
    size_t max_cache_size,
    base::TimeDelta max_cache_age)
    : max_cache_size_(max_cache_size), max_cache_age_(max_cache_age) {
  if (history_service) {
    history_observation_.Observe(history_service);
  }
  auto database_task_runner = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});
  db_ = db_provider->GetDB<CacheEntryWithMetadata>(
      leveldb_proto::ProtoDbType::AUTOFILL_AI_MODEL_CACHE,
      profile_path.Append(kAutofillAiModelCacheDatabaseFileName),
      database_task_runner);

  db_->Init(base::BindRepeating(&AutofillAiModelCacheImpl::OnDatabaseInit,
                                weak_ptr_factory_.GetWeakPtr()));
}

AutofillAiModelCacheImpl::~AutofillAiModelCacheImpl() = default;

void AutofillAiModelCacheImpl::Update(
    FormSignature form_signature,
    ModelResponse response,
    base::span<const FieldIdentifier> field_identifiers) {
  CHECK_EQ(base::checked_cast<size_t>(response.field_responses_size()),
           field_identifiers.size());

  CacheEntryWithMetadata entry_with_metadata;
  entry_with_metadata.mutable_field_identifiers()->Reserve(
      field_identifiers.size());
  for (const FieldIdentifier& field_identifier : field_identifiers) {
    auto proto = entry_with_metadata.mutable_field_identifiers()->Add();
    proto->set_field_signature(*field_identifier.signature);
    proto->set_field_rank_in_signature_group(
        field_identifier.rank_in_signature_group);
  }

  *entry_with_metadata.mutable_server_response() = std::move(response);
  entry_with_metadata.set_creation_time(SerializeTime(base::Time::Now()));
  UpdateInDatabase(form_signature, entry_with_metadata);
  in_memory_cache_[form_signature] = std::move(entry_with_metadata);
  TrimEntries();
}

bool AutofillAiModelCacheImpl::Contains(FormSignature form_signature) const {
  return GetRawEntry(form_signature).has_value();
}

void AutofillAiModelCacheImpl::Erase(FormSignature form_signature) {
  in_memory_cache_.erase(form_signature);
  EraseInDatabase({form_signature});
}

base::flat_map<AutofillAiModelCache::FieldIdentifier,
               AutofillAiModelCache::FieldPrediction>
AutofillAiModelCacheImpl::GetFieldPredictions(
    FormSignature form_signature) const {
  base::optional_ref<const CacheEntryWithMetadata> cache_entry =
      GetRawEntry(form_signature);
  if (!cache_entry || (cache_entry->field_identifiers_size() !=
                       cache_entry->server_response().field_responses_size())) {
    return {};
  }

  const optimization_guide::proto::AutofillAiTypeResponse& server_response =
      cache_entry->server_response();
  std::vector<std::pair<FieldIdentifier, FieldPrediction>> result;
  result.reserve(cache_entry->field_identifiers_size());
  for (int i = 0; i < cache_entry->field_identifiers_size(); ++i) {
    const AutofillAiModelCacheEntryWithMetadata_FieldIdentifier& identifier =
        cache_entry->field_identifiers(i);
    const optimization_guide::proto::FieldTypeResponse& prediction =
        server_response.field_responses(i);

    // TODO(crbug.com/389625753): Either implement format strings properly and
    // include more types than just date in the model or remove them completely.
    std::optional<AutofillFormatString> format_string;
    if (std::u16string format_string_u16 =
            base::UTF8ToUTF16(prediction.formatting_meta());
        data_util::IsValidDateFormat(format_string_u16)) {
      format_string.emplace(std::move(format_string_u16),
                            FormatString_Type_DATE);
    }
    result.emplace_back(
        FieldIdentifier{
            .signature = FieldSignature(identifier.field_signature()),
            .rank_in_signature_group =
                identifier.field_rank_in_signature_group()},
        FieldPrediction(
            ToSafeFieldType(prediction.field_type(), NO_SERVER_DATA),
            std::move(format_string)));
  }
  return std::move(result);
}

std::map<FormSignature, AutofillAiModelCache::CacheEntryWithMetadata>
AutofillAiModelCacheImpl::GetAllEntries() const {
  return in_memory_cache_;
}

void AutofillAiModelCacheImpl::OnHistoryDeletions(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  if (deletion_info.is_from_expiration() || !deletion_info.IsAllHistory() ||
      !db_initialized_) {
    return;
  }
  in_memory_cache_.clear();
  db_->UpdateEntriesWithRemoveFilter(
      std::make_unique<Database::KeyEntryVector>(),
      base::BindRepeating([](const std::string&) { return true; }),
      base::DoNothing());
}

base::optional_ref<const AutofillAiModelCache::CacheEntryWithMetadata>
AutofillAiModelCacheImpl::GetRawEntry(FormSignature form_signature) const {
  auto it = in_memory_cache_.find(form_signature);
  if (it == in_memory_cache_.end() ||
      it->second.creation_time() <
          SerializeTime(base::Time::Now() - max_cache_age_)) {
    return std::nullopt;
  }
  return it->second;
}

void AutofillAiModelCacheImpl::TrimEntries() {
  // Transform the cache into an ordered list of (creation_time, FormSignature)
  // pairs.
  std::vector<std::pair<int64_t, FormSignature>> entries_by_creation_time =
      base::ToVector(in_memory_cache_,
                     [](const std::pair<FormSignature, CacheEntryWithMetadata>&
                            map_entry) {
                       const auto& [signature, cache_entry] = map_entry;
                       return std::pair(cache_entry.creation_time(), signature);
                     });
  std::ranges::sort(entries_by_creation_time);

  // Entries with a time stamp smaller than `cut_off_time` should be removed.
  const int64_t cut_off_time =
      SerializeTime(base::Time::Now() - max_cache_age_);
  const size_t deletions_due_to_time_cutoff = std::distance(
      entries_by_creation_time.begin(),
      std::ranges::lower_bound(entries_by_creation_time, cut_off_time, {},
                               &std::pair<int64_t, FormSignature>::first));
  // The remaining cache should at most have size `max_cache_size_`.
  size_t deletions_overall = deletions_due_to_time_cutoff;
  if (entries_by_creation_time.size() - deletions_overall > max_cache_size_) {
    deletions_overall = entries_by_creation_time.size() - max_cache_size_;
  }

  if (deletions_overall == 0) {
    return;
  }

  std::vector<FormSignature> signatures_to_remove;
  signatures_to_remove.reserve(deletions_overall);
  for (const auto [_, signature_to_delete] :
       base::span(entries_by_creation_time).first(deletions_overall)) {
    in_memory_cache_.erase(signature_to_delete);
    signatures_to_remove.push_back(signature_to_delete);
  }
  EraseInDatabase(signatures_to_remove);
}

void AutofillAiModelCacheImpl::UpdateInDatabase(
    FormSignature form_signature,
    const CacheEntryWithMetadata& entry) {
  if (!db_initialized_) {
    return;
  }
  auto entries_to_save = std::make_unique<Database::KeyEntryVector>();
  entries_to_save->emplace_back(base::NumberToString(*form_signature), entry);
  auto keys_to_remove = std::make_unique<std::vector<std::string>>();
  db_->UpdateEntries(std::move(entries_to_save), std::move(keys_to_remove),
                     /*callback=*/base::DoNothing());
}

void AutofillAiModelCacheImpl::EraseInDatabase(
    base::span<const FormSignature> form_signatures) {
  if (!db_initialized_) {
    return;
  }

  auto entries_to_save = std::make_unique<Database::KeyEntryVector>();
  auto keys_to_remove = std::make_unique<std::vector<std::string>>();
  *keys_to_remove = base::ToVector(
      form_signatures,
      [](FormSignature signature) { return base::NumberToString(*signature); });
  db_->UpdateEntries(std::move(entries_to_save), std::move(keys_to_remove),
                     /*callback=*/base::DoNothing());
}

void AutofillAiModelCacheImpl::OnDatabaseInit(
    leveldb_proto::Enums::InitStatus status) {
  const bool success = status == leveldb_proto::Enums::InitStatus::kOK;
  base::UmaHistogramBoolean("Autofill.AutofillAi.ModelCache.InitSuccess",
                            success);
  if (!success) {
    return;
  }
  db_initialized_ = true;
  db_->LoadKeysAndEntries(base::BindOnce(
      &AutofillAiModelCacheImpl::OnDatabaseLoadKeysAndEntries, GetWeakPtr()));
}

void AutofillAiModelCacheImpl::OnDatabaseLoadKeysAndEntries(
    bool success,
    std::unique_ptr<std::map<std::string, CacheEntryWithMetadata>> entries) {
  if (!success) {
    db_initialized_ = false;
    return;
  }

  std::vector<std::string> malformed_signatures;
  for (auto& [signature_str, entry] : *entries) {
    uint64_t signature_uint64;
    if (!base::StringToUint64(signature_str, &signature_uint64)) {
      malformed_signatures.push_back(std::move(signature_str));
      continue;
    }
    in_memory_cache_[FormSignature(signature_uint64)] = std::move(entry);
  }
  if (!malformed_signatures.empty()) {
    auto entries_to_save = std::make_unique<Database::KeyEntryVector>();
    auto keys_to_remove = std::make_unique<std::vector<std::string>>();
    *keys_to_remove = std::move(malformed_signatures);
    db_->UpdateEntries(std::move(entries_to_save), std::move(keys_to_remove),
                       /*callback=*/base::DoNothing());
  }
  TrimEntries();
}

}  // namespace autofill
