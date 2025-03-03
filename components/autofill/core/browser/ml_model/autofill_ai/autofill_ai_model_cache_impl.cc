// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ml_model/autofill_ai/autofill_ai_model_cache_impl.h"

#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool.h"
#include "components/autofill/core/common/signatures.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/leveldb_proto/public/proto_database_provider.h"

namespace autofill {

AutofillAiModelCacheImpl::AutofillAiModelCacheImpl(
    leveldb_proto::ProtoDatabaseProvider* db_provider,
    const base::FilePath& profile_path) {
  auto database_task_runner = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});
  db_ = db_provider->GetDB<CacheEntryWithMetadata>(
      leveldb_proto::ProtoDbType::STRIKE_DATABASE,
      profile_path.Append(kAutofillAiModelCacheDatabaseFileName),
      database_task_runner);

  db_->Init(base::BindRepeating(&AutofillAiModelCacheImpl::OnDatabaseInit,
                                weak_ptr_factory_.GetWeakPtr()));
}

AutofillAiModelCacheImpl::~AutofillAiModelCacheImpl() = default;

void AutofillAiModelCacheImpl::Update(FormSignature form_signature,
                                      CacheEntry entry) {
  CacheEntryWithMetadata entry_with_metadata;
  *entry_with_metadata.mutable_server_response() = std::move(entry);
  entry_with_metadata.set_creation_time(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
  UpdateInDatabase(form_signature, entry_with_metadata);
  in_memory_cache_[form_signature] = std::move(entry_with_metadata);
}

bool AutofillAiModelCacheImpl::Contains(FormSignature form_signature) const {
  return in_memory_cache_.contains(form_signature);
}

void AutofillAiModelCacheImpl::UpdateInDatabase(
    FormSignature form_signature,
    const CacheEntryWithMetadata& entry) {
  if (!db_initialized_) {
    // TODO(crbug.com/389631477): Emit metric about error.
    return;
  }
  auto entries_to_save = std::make_unique<
      leveldb_proto::ProtoDatabase<CacheEntryWithMetadata>::KeyEntryVector>();
  entries_to_save->emplace_back(base::NumberToString(*form_signature), entry);
  auto keys_to_remove = std::make_unique<std::vector<std::string>>();
  db_->UpdateEntries(std::move(entries_to_save), std::move(keys_to_remove),
                     /*callback=*/base::DoNothing());
}

void AutofillAiModelCacheImpl::OnDatabaseInit(
    leveldb_proto::Enums::InitStatus status) {
  if (status != leveldb_proto::Enums::InitStatus::kOK) {
    // TODO(crbug.com/389631477): Emit metric about error.
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

  for (auto& [signature_str, entry] : *entries) {
    uint64_t signature_uint64;
    if (!base::StringToUint64(signature_str, &signature_uint64)) {
      continue;
    }
    in_memory_cache_[FormSignature(signature_uint64)] = std::move(entry);
  }
}

}  // namespace autofill
