// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_LOCAL_PAGE_ENTITIES_METADATA_PROVIDER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_LOCAL_PAGE_ENTITIES_METADATA_PROVIDER_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/optimization_guide/core/entity_metadata_provider.h"
#include "components/optimization_guide/proto/page_entities_metadata.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace optimization_guide {

// Provides EntityMetadata given an entity id by looking up entries in a local
// database on-disk.
class LocalPageEntitiesMetadataProvider : public EntityMetadataProvider {
 public:
  LocalPageEntitiesMetadataProvider();
  ~LocalPageEntitiesMetadataProvider() override;
  LocalPageEntitiesMetadataProvider(const LocalPageEntitiesMetadataProvider&) =
      delete;
  LocalPageEntitiesMetadataProvider& operator=(
      const LocalPageEntitiesMetadataProvider&) = delete;

  // Initializes this class, setting |database_| and |background_task_runner_|.
  void Initialize(
      leveldb_proto::ProtoDatabaseProvider* database_provider,
      const base::FilePath& database_dir,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner);

  // Directly sets |database_| and |background_task_runner_| for tests.
  void InitializeForTesting(
      std::unique_ptr<
          leveldb_proto::ProtoDatabase<proto::EntityMetadataStorage>> database,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner);

  // EntityMetadataProvider:
  void GetMetadataForEntityId(
      const std::string& entity_id,
      EntityMetadataRetrievedCallback callback) override;

 private:
  void OnDatabaseInitialized(leveldb_proto::Enums::InitStatus status);
  void OnGotEntry(const std::string& entity_id,
                  EntityMetadataRetrievedCallback callback,
                  bool success,
                  std::unique_ptr<proto::EntityMetadataStorage> entry);

  std::unique_ptr<leveldb_proto::ProtoDatabase<proto::EntityMetadataStorage>>
      database_;

  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<LocalPageEntitiesMetadataProvider> weak_ptr_factory_{
      this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_LOCAL_PAGE_ENTITIES_METADATA_PROVIDER_H_