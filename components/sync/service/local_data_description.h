// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_LOCAL_DATA_DESCRIPTION_H_
#define COMPONENTS_SYNC_SERVICE_LOCAL_DATA_DESCRIPTION_H_

#include <string>
#include <vector>

#include "components/sync/base/model_type.h"

namespace syncer {

struct LocalDataDescription {
  ModelType type = syncer::UNSPECIFIED;
  // Actual count of local items.
  int item_count = 0;
  // Preferably contains up to 3 distinct domains corresponding to some of the
  // local items, to be used for a preview.
  std::vector<std::string> domains;
  // Count of distinct domains for preview.
  // Note: This may be different from the count of items(`item_count`), since a
  // user might have, for e.g., multiple bookmarks or passwords for the same
  // domain.
  int domain_count = 0;

  LocalDataDescription();
  LocalDataDescription(ModelType type,
                       int item_count,
                       const std::vector<std::string>& domains,
                       int domain_count);
  LocalDataDescription(const LocalDataDescription&);
  LocalDataDescription& operator=(const LocalDataDescription&);
  LocalDataDescription(LocalDataDescription&&);
  LocalDataDescription& operator=(LocalDataDescription&&);
  ~LocalDataDescription();
};

bool operator==(const LocalDataDescription& lhs,
                const LocalDataDescription& rhs);
bool operator!=(const LocalDataDescription& lhs,
                const LocalDataDescription& rhs);

// gmock printer helper.
void PrintTo(const LocalDataDescription& local_data_description,
             std::ostream* os);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_LOCAL_DATA_DESCRIPTION_H_
