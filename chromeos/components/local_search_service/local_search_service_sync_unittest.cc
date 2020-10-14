// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "base/test/task_environment.h"
#include "chromeos/components/local_search_service/index_sync.h"
#include "chromeos/components/local_search_service/local_search_service_sync.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace local_search_service {

class LocalSearchServiceSyncTest : public testing::Test {
 protected:
  LocalSearchServiceSync service_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::DEFAULT,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED};
};

TEST_F(LocalSearchServiceSyncTest, GetLinearMapSearch) {
  IndexSync* const index = service_.GetIndexSync(
      IndexId::kCrosSettings, Backend::kLinearMap, nullptr /* local_state */);
  CHECK(index);

  EXPECT_EQ(index->GetSizeSync(), 0u);
}

TEST_F(LocalSearchServiceSyncTest, GetInvertedIndexSearch) {
  IndexSync* const index =
      service_.GetIndexSync(IndexId::kCrosSettings, Backend::kInvertedIndex,
                            nullptr /* local_state */);
  CHECK(index);

  EXPECT_EQ(index->GetSizeSync(), 0u);
}

}  // namespace local_search_service
}  // namespace chromeos
