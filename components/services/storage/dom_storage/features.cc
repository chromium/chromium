// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/features.h"

#include "base/feature_list.h"
#include "base/notreached.h"

namespace storage {

BASE_FEATURE(kDomStorageSqlite, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDomStorageSqliteInMemory, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDomStorageSqliteNewDatabases, base::FEATURE_DISABLED_BY_DEFAULT);

constexpr base::FeatureParam<DomStorageSqliteRolloutStage>::Option
    kDomStorageSqliteNewDatabasesStages[] = {
        {DomStorageSqliteRolloutStage::kUseLevelDbOnly, "UseLevelDbOnly"},
        {DomStorageSqliteRolloutStage::kUseLevelDbAsControl,
         "UseLevelDbAsControl"},
        {DomStorageSqliteRolloutStage::kUseSqliteForNewDatabases,
         "UseSqliteForNewDatabases"},
        {DomStorageSqliteRolloutStage::kUseSqliteOnly, "UseSqliteOnly"},
};

BASE_FEATURE_ENUM_PARAM(DomStorageSqliteRolloutStage,
                        kDomStorageSqliteNewDatabasesStage,
                        &kDomStorageSqliteNewDatabases,
                        DomStorageSqliteRolloutStage::kUseLevelDbOnly,
                        &kDomStorageSqliteNewDatabasesStages);

DomStorageSqliteRolloutStage GetSqliteRolloutStage(bool in_memory) {
  if (base::FeatureList::IsEnabled(kDomStorageSqlite)) {
    return DomStorageSqliteRolloutStage::kUseSqliteOnly;
  }
  if (in_memory) {
    return base::FeatureList::IsEnabled(kDomStorageSqliteInMemory)
               ? DomStorageSqliteRolloutStage::kUseSqliteOnly
               : DomStorageSqliteRolloutStage::kUseLevelDbOnly;
  }
  if (base::FeatureList::IsEnabled(kDomStorageSqliteNewDatabases)) {
    return kDomStorageSqliteNewDatabasesStage.Get();
  }
  return DomStorageSqliteRolloutStage::kUseLevelDbOnly;
}

bool IsExperimentalRolloutStage(DomStorageSqliteRolloutStage stage) {
  return stage == DomStorageSqliteRolloutStage::kUseLevelDbAsControl ||
         stage == DomStorageSqliteRolloutStage::kUseSqliteForNewDatabases;
}

bool ShouldUseSqlite(DomStorageSqliteRolloutStage stage, bool leveldb_exists) {
  switch (stage) {
    case DomStorageSqliteRolloutStage::kUseLevelDbOnly:
    case DomStorageSqliteRolloutStage::kUseLevelDbAsControl:
      return false;
    case DomStorageSqliteRolloutStage::kUseSqliteForNewDatabases:
      return !leveldb_exists;
    case DomStorageSqliteRolloutStage::kUseSqliteOnly:
      return true;
  }
  NOTREACHED();
}

bool ShouldWriteExpTag(DomStorageSqliteRolloutStage stage,
                       bool leveldb_exists) {
  return stage == DomStorageSqliteRolloutStage::kUseLevelDbAsControl &&
         !leveldb_exists;
}

}  // namespace storage
