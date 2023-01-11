// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_PERSISTENT_AVAILABILITY_STORE_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_PERSISTENT_AVAILABILITY_STORE_H_

#include <stdint.h>

#include <map>
#include <memory>

#include "base/functional/callback_forward.h"
#include "components/feature_engagement/internal/proto/availability.pb.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/leveldb_proto/public/proto_database.h"

namespace base {
class FilePath;
}  // namespace base

namespace feature_engagement {

// An PersistentAvailabilityStore provides a way to load and update the
// availability date for all registered features.
class PersistentAvailabilityStore {
 public:
  // Invoked when the availability data has finished loading, and whether the
  // load was a success. In the case of a failure, the map argument will be
  // empty. The value for each entry in the map is the day number since epoch
  // (1970-01-01) in the local timezone for when the particular feature was made
  // available.
  using OnLoadedCallback = base::OnceCallback<
      void(bool success, std::unique_ptr<std::map<std::string, uint32_t>>)>;

  PersistentAvailabilityStore(const PersistentAvailabilityStore&) = delete;
  PersistentAvailabilityStore& operator=(const PersistentAvailabilityStore&) =
      delete;

  // Loads the availability data, updates the DB with newly enabled features,
  // deletes features that are not enabled anymore, and asynchronously invokes
  // |on_loaded_callback| with the result. The result will mirror the content
  // of the database.
  // The |feature_filter| is used to filter the data from the DB and ensure
  // that only enabled features listed in this filter are tracked. For enabled
  // features that are in the |feature_filter|, but not in the DB, they are
  // tracked as new entries with the |current_day| as the availability day.
  static void LoadAndUpdateStore(
      const base::FilePath& storage_dir,
      std::unique_ptr<leveldb_proto::ProtoDatabase<Availability>> db,
      FeatureVector feature_filter,
      OnLoadedCallback on_loaded_callback,
      uint32_t current_day);

 private:
  PersistentAvailabilityStore() = default;
  ~PersistentAvailabilityStore() = default;
};

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_PERSISTENT_AVAILABILITY_STORE_H_
