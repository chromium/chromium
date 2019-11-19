// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/persistent_availability_store.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "components/feature_engagement/internal/proto/availability.pb.h"
#include "components/feature_engagement/internal/stats.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/leveldb_proto/public/proto_database.h"

namespace feature_engagement {

namespace {

using KeyAvailabilityPair = std::pair<std::string, Availability>;
using KeyAvailabilityList = std::vector<KeyAvailabilityPair>;

void OnDBUpdateComplete(
    std::unique_ptr<leveldb_proto::ProtoDatabase<Availability>> db,
    PersistentAvailabilityStore::OnLoadedCallback on_loaded_callback,
    std::unique_ptr<std::map<std::string, uint32_t>> feature_availabilities,
    bool success) {
  stats::RecordDbUpdate(success, stats::StoreType::AVAILABILITY_STORE);
  std::move(on_loaded_callback).Run(success, std::move(feature_availabilities));
}

void OnDBLoadComplete(
    std::unique_ptr<leveldb_proto::ProtoDatabase<Availability>> db,
    FeatureVector feature_filter,
    PersistentAvailabilityStore::OnLoadedCallback on_loaded_callback,
    uint32_t current_day,
    bool success,
    std::unique_ptr<std::vector<Availability>> availabilities) {
  stats::RecordAvailabilityDbLoadEvent(success);
  if (!success) {
    std::move(on_loaded_callback)
        .Run(false, std::make_unique<std::map<std::string, uint32_t>>());
    return;
  }

  // Create map from feature name to Feature.
  std::map<std::string, const base::Feature*> feature_mapping;
  for (const base::Feature* feature : feature_filter) {
    DCHECK(feature_mapping.find(feature->name) == feature_mapping.end());
    feature_mapping[feature->name] = feature;
  }

  // Find all availabilities from DB and find out what should be deleted.
  auto feature_availabilities =
      std::make_unique<std::map<std::string, uint32_t>>();
  auto deletes = std::make_unique<std::vector<std::string>>();
  for (auto& availability : *availabilities) {
    // Check if in |feature_filter|.
    if (feature_mapping.find(availability.feature_name()) ==
        feature_mapping.end()) {
      deletes->push_back(availability.feature_name());
      continue;
    }

    // Check if enabled.
    const base::Feature* feature = feature_mapping[availability.feature_name()];
    if (!base::FeatureList::IsEnabled(*feature)) {
      deletes->push_back(availability.feature_name());
      continue;
    }

    // Both in |feature_filter| and is enabled, so keep around.
    feature_availabilities->insert(
        std::make_pair(feature->name, availability.day()));
    DVLOG(2) << "Keeping availability for " << feature->name << " @ "
             << availability.day();
  }

  // Find features from |feature_filter| that are enabled, but not in DB yet.
  auto additions = std::make_unique<KeyAvailabilityList>();
  for (const base::Feature* feature : feature_filter) {
    // Check if already in DB.
    if (feature_availabilities->find(feature->name) !=
        feature_availabilities->end())
      continue;

    // Check if enabled.
    if (!base::FeatureList::IsEnabled(*feature))
      continue;

    // Both in feature filter, and is enabled, but not in DB, so add to DB.
    Availability availability;
    availability.set_feature_name(feature->name);
    availability.set_day(current_day);
    additions->push_back({feature->name, std::move(availability)});
    // Since it will be written to the DB, also add to the callback result.
    feature_availabilities->insert({feature->name, current_day});

    DVLOG(2) << "Adding availability for " << feature->name << " @ "
             << current_day;
  }

  // Write all changes to the DB.
  auto* db_ptr = db.get();
  db_ptr->UpdateEntries(std::move(additions), std::move(deletes),
                        base::BindOnce(&OnDBUpdateComplete, std::move(db),
                                       std::move(on_loaded_callback),
                                       std::move(feature_availabilities)));
}

void OnDBInitComplete(
    std::unique_ptr<leveldb_proto::ProtoDatabase<Availability>> db,
    FeatureVector feature_filter,
    PersistentAvailabilityStore::OnLoadedCallback on_loaded_callback,
    uint32_t current_day,
    leveldb_proto::Enums::InitStatus status) {
  bool success = status == leveldb_proto::Enums::InitStatus::kOK;
  stats::RecordDbInitEvent(success, stats::StoreType::AVAILABILITY_STORE);

  if (!success) {
    std::move(on_loaded_callback)
        .Run(false, std::make_unique<std::map<std::string, uint32_t>>());
    return;
  }

  auto* db_ptr = db.get();
  db_ptr->LoadEntries(base::BindOnce(
      &OnDBLoadComplete, std::move(db), std::move(feature_filter),
      std::move(on_loaded_callback), current_day));
}

}  // namespace

// static
void PersistentAvailabilityStore::LoadAndUpdateStore(
    const base::FilePath& storage_dir,
    std::unique_ptr<leveldb_proto::ProtoDatabase<Availability>> db,
    FeatureVector feature_filter,
    PersistentAvailabilityStore::OnLoadedCallback on_loaded_callback,
    uint32_t current_day) {
  auto* db_ptr = db.get();
  db_ptr->Init(base::BindOnce(&OnDBInitComplete, std::move(db),
                              std::move(feature_filter),
                              std::move(on_loaded_callback), current_day));
}

}  // namespace feature_engagement
