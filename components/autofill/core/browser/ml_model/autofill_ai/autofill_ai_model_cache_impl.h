// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_AUTOFILL_AI_AUTOFILL_AI_MODEL_CACHE_IMPL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_AUTOFILL_AI_AUTOFILL_AI_MODEL_CACHE_IMPL_H_

#include <map>
#include <memory>
#include <string>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/types/optional_ref.h"
#include "components/autofill/core/browser/ml_model/autofill_ai/autofill_ai_model_cache.h"
#include "components/autofill/core/browser/proto/autofill_ai_model_cache.pb.h"
#include "components/autofill/core/common/signatures.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/leveldb_proto/public/proto_database.h"

namespace leveldb_proto {
class ProtoDatabaseProvider;
}  // namespace leveldb_proto

namespace autofill {

// TODO(crbug.com/389631477): Investigate adding this to the snapshot file
// collector.
inline constexpr base::FilePath::StringViewType
    kAutofillAiModelCacheDatabaseFileName =
        FILE_PATH_LITERAL("AutofillAiModelCache");

// `AutofillAiModelCacheImpl` implements `AutofillAiModelCache` using a
// LevelDB as a persistence layer and a map as an in-memory cache to allow
// synchronous access.
class AutofillAiModelCacheImpl : public AutofillAiModelCache,
                                 history::HistoryServiceObserver {
 public:
  AutofillAiModelCacheImpl(history::HistoryService* history_service,
                           leveldb_proto::ProtoDatabaseProvider* db_provider,
                           const base::FilePath& profile_path,
                           size_t max_cache_size,
                           base::TimeDelta max_cache_age);
  AutofillAiModelCacheImpl(const AutofillAiModelCacheImpl&) = delete;
  AutofillAiModelCacheImpl& operator=(const AutofillAiModelCacheImpl&) = delete;
  AutofillAiModelCacheImpl(AutofillAiModelCacheImpl&&) = delete;
  AutofillAiModelCacheImpl& operator=(AutofillAiModelCacheImpl&&) = delete;
  ~AutofillAiModelCacheImpl() override;

  // AutofillAiModelCache:
  void Update(FormSignature form_signature,
              ModelResponse response,
              base::span<const FieldIdentifier> field_identifiers) override;
  bool Contains(FormSignature form_signature) const override;
  void Erase(FormSignature form_signature) override;
  std::map<FormSignature, CacheEntryWithMetadata> GetAllEntries()
      const override;
  base::flat_map<FieldIdentifier, FieldPrediction> GetFieldPredictions(
      FormSignature form_signature) const override;

  // history::HistoryServiceObserver:
  void OnHistoryDeletions(history::HistoryService* history_service,
                          const history::DeletionInfo& deletion_info) override;

 private:
  using Database = leveldb_proto::ProtoDatabase<CacheEntryWithMetadata>;

  // Returns the entry corresponding to `form_signature` in the form that it
  // is saved in the database.
  base::optional_ref<const CacheEntryWithMetadata> GetRawEntry(
      FormSignature form_signature) const LIFETIME_BOUND;

  // Removes expired cache entries and limits the cache size to
  // `max_cache_size_` by removing the oldest entries.
  void TrimEntries();

  void UpdateInDatabase(FormSignature form_signature,
                        const CacheEntryWithMetadata& entry);

  void EraseInDatabase(base::span<const FormSignature> form_signatures);

  void OnDatabaseInit(leveldb_proto::Enums::InitStatus status);
  void OnDatabaseLoadKeysAndEntries(
      bool success,
      std::unique_ptr<std::map<std::string, CacheEntryWithMetadata>> entries);

  base::WeakPtr<AutofillAiModelCacheImpl> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  const size_t max_cache_size_;
  const base::TimeDelta max_cache_age_;

  // An in-memory cache that allows for synchronous access. Should contain the
  // same content as the database.
  std::map<FormSignature, CacheEntryWithMetadata> in_memory_cache_;

  // The database. Use only if `db_initialized_` is `true`.
  std::unique_ptr<Database> db_;

  // Whether the database has been initialized successfully.
  bool db_initialized_ = false;

  // Observes the HistoryService to listen for deletions.
  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      history_observation_{this};

  base::WeakPtrFactory<AutofillAiModelCacheImpl> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_AUTOFILL_AI_AUTOFILL_AI_MODEL_CACHE_IMPL_H_
