// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_STORE_UPDATE_DATA_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_STORE_UPDATE_DATA_H_

#include <string>

#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/version.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace optimization_guide {
namespace proto {
class Hint;
class PredictionModel;
class StoreEntry;
}  // namespace proto

using EntryVector =
    leveldb_proto::ProtoDatabase<proto::StoreEntry>::KeyEntryVector;

// Holds hint or prediction model data for updating the OptimizationGuideStore.
class StoreUpdateData {
 public:
  StoreUpdateData(const StoreUpdateData&) = delete;
  StoreUpdateData& operator=(const StoreUpdateData&) = delete;

  ~StoreUpdateData();

  // Creates an update data object for a component hint update.
  static std::unique_ptr<StoreUpdateData> CreateComponentStoreUpdateData(
      const base::Version& component_version);

  // Creates an update data object for a fetched hint update.
  static std::unique_ptr<StoreUpdateData> CreateFetchedStoreUpdateData(
      base::Time fetch_update_time);

  // Creates an update data object for a prediction model update.
  static std::unique_ptr<StoreUpdateData> CreatePredictionModelStoreUpdateData(
      base::Time expiry_time);

  // Returns the component version of a component hint update.
  const absl::optional<base::Version> component_version() const {
    return component_version_;
  }

  // Returns the next update time for the entries in the store update.
  const absl::optional<base::Time> update_time() const { return update_time_; }

  // Returns the expiry time for the hints in a fetched hint update.
  const absl::optional<base::Time> expiry_time() const { return expiry_time_; }

  // Moves |hint| into this update data. After MoveHintIntoUpdateData() is
  // called, |hint| is no longer valid.
  void MoveHintIntoUpdateData(proto::Hint&& hint);

  // Copies |prediction_model| into this update data.
  void CopyPredictionModelIntoUpdateData(
      const proto::PredictionModel& prediction_model);

  // Returns the store entry updates along with ownership to them.
  std::unique_ptr<EntryVector> TakeUpdateEntries();

 private:
  StoreUpdateData(absl::optional<base::Version> component_version,
                  absl::optional<base::Time> fetch_update_time,
                  absl::optional<base::Time> expiry_time);
  explicit StoreUpdateData(base::Time expiry_time);

  // The component version of the update data for a component update.
  absl::optional<base::Version> component_version_;

  // The time when the entries in this update need to be updated.
  absl::optional<base::Time> update_time_;

  // The time when entries in this update expire.
  absl::optional<base::Time> expiry_time_;

  // The prefix to add to the key of every store entry. It is set
  // during construction for appropriate type of update.
  std::string entry_key_prefix_;

  // The vector of entries to save.
  std::unique_ptr<EntryVector> entries_to_save_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_STORE_UPDATE_DATA_H_
