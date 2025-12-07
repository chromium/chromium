// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/resource_tracker.h"

#include <utility>

#include "base/byte_count.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "content/public/browser/global_request_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class ResourceTrackerTest : public testing::Test {
 public:
  ResourceTrackerTest() = default;

  void StartResourceLoad(int resource_id, bool is_complete = false) {
    CreateResourceUpdate(resource_id, /*delta_bytes=*/base::ByteCount(0),
                         /*is_complete=*/is_complete);
  }

  void AdvanceResourceLoad(int resource_id, base::ByteCount bytes) {
    CreateResourceUpdate(resource_id, /*delta_bytes=*/bytes,
                         /*is_complete=*/false);
  }

  void CompleteResourceLoad(int resource_id) {
    CreateResourceUpdate(resource_id, /*delta_bytes=*/base::ByteCount(0),
                         /*is_complete=*/true);
  }

  bool HasUnfinishedResource(int resource_id) {
    return resource_tracker_.unfinished_resources().find(
               content::GlobalRequestID(process_id_, resource_id)) !=
           resource_tracker_.unfinished_resources().end();
  }

  base::ByteCount GetUnfinishedResourceBytes(int resource_id) {
    return resource_tracker_.unfinished_resources()
        .find(content::GlobalRequestID(process_id_, resource_id))
        ->second->delta_bytes;
  }

  bool HasPreviousUpdateForResource(int resource_id) {
    return resource_tracker_.HasPreviousUpdateForResource(
        content::GlobalRequestID(process_id_, resource_id));
  }

  base::ByteCount GetPreviousResourceUpdateDelta(int resource_id) {
    return resource_tracker_
        .GetPreviousUpdateForResource(
            content::GlobalRequestID(process_id_, resource_id))
        ->delta_bytes;
  }

 private:
  void CreateResourceUpdate(int request_id,
                            base::ByteCount delta_bytes,
                            bool is_complete) {
    std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr> resources;
    auto resource_data_update =
        page_load_metrics::mojom::ResourceDataUpdate::New();
    resource_data_update->request_id = request_id;
    resource_data_update->delta_bytes = delta_bytes;
    resource_data_update->is_complete = is_complete;
    resources.push_back(std::move(resource_data_update));
    resource_tracker_.UpdateResourceDataUse(process_id_, resources);
  }

  const int process_id_ = 0;

  page_load_metrics::ResourceTracker resource_tracker_;
};

// Verifies that resources are added to and removed from the map
// of ongoing resource loads as expected.
TEST_F(ResourceTrackerTest, UnfinishedResourceMap) {
  StartResourceLoad(/*resource_id=*/0);
  StartResourceLoad(/*resource_id=*/1);
  StartResourceLoad(/*resource_id=*/2);

  // Verify completed resources are not stored in the unfinished map.
  EXPECT_TRUE(HasUnfinishedResource(/*resource_id=*/0));
  CompleteResourceLoad(/*resource_id=*/0);
  EXPECT_FALSE(HasUnfinishedResource(/*resource_id=*/0));

  // Verify that resources receiving multiple updates are not removed from the
  // map.
  AdvanceResourceLoad(/*resource_id=*/1, /*bytes=*/base::ByteCount(10));
  AdvanceResourceLoad(/*resource_id=*/1, /*bytes=*/base::ByteCount(20));
  EXPECT_TRUE(HasUnfinishedResource(/*resource_id=*/1));
  CompleteResourceLoad(/*resource_id=*/1);
  EXPECT_FALSE(HasUnfinishedResource(/*resource_id=*/1));

  // Verify the unfinished map stores the most recent resource update.
  EXPECT_EQ(base::ByteCount(0), GetUnfinishedResourceBytes(/*resource_id=*/2));
  AdvanceResourceLoad(/*resource_id=*/2, /*bytes=*/base::ByteCount(10));
  EXPECT_EQ(base::ByteCount(10), GetUnfinishedResourceBytes(/*resource_id=*/2));
  AdvanceResourceLoad(/*resource_id=*/2, /*bytes=*/base::ByteCount(20));
  AdvanceResourceLoad(/*resource_id=*/2, /*bytes=*/base::ByteCount(30));
  EXPECT_EQ(base::ByteCount(30), GetUnfinishedResourceBytes(/*resource_id=*/2));
}

// Verifies that resources are added to and removed from the map
// of previous resource updates as expected.
TEST_F(ResourceTrackerTest, PreviousUpdateResourceMap) {
  StartResourceLoad(/*resource_id=*/0);
  StartResourceLoad(/*resource_id=*/1);
  EXPECT_FALSE(HasPreviousUpdateForResource(/*resource_id=*/0));
  EXPECT_FALSE(HasPreviousUpdateForResource(/*resource_id=*/1));

  AdvanceResourceLoad(/*resource_id=*/1, /*bytes=*/base::ByteCount(10));
  EXPECT_TRUE(HasPreviousUpdateForResource(/*resource_id=*/1));

  // Previous resource update should only be available for resources
  // who received resource updates in the previous call to
  // ResourceTracker::UpdateResourceDataUse(). resource_id "1" should not be
  // available in this case.
  AdvanceResourceLoad(/*resource_id=*/0, /*bytes=*/base::ByteCount(0));
  EXPECT_FALSE(HasPreviousUpdateForResource(/*resource_id=*/1));

  // The update should not be available because the load for resource_id "1" was
  // still ongoing. This should hold the data for the last update, 10 bytes.
  AdvanceResourceLoad(/*resource_id=*/1, /*bytes=*/base::ByteCount(20));
  EXPECT_TRUE(HasPreviousUpdateForResource(/*resource_id=*/1));
  EXPECT_EQ(base::ByteCount(10),
            GetPreviousResourceUpdateDelta(/*resource_id=*/1));

  // Verify previous resource update is available for newly complete resources.
  CompleteResourceLoad(/*resource_id=*/1);
  EXPECT_TRUE(HasPreviousUpdateForResource(/*resource_id=*/1));
  EXPECT_EQ(base::ByteCount(20),
            GetPreviousResourceUpdateDelta(/*resource_id=*/1));

  // Verify this completed resource update is removed once other resources are
  // loaded.
  CompleteResourceLoad(/*resource_id=*/0);
  EXPECT_FALSE(HasPreviousUpdateForResource(/*resource_id=*/1));
}

TEST_F(ResourceTrackerTest, SingleUpdateResourceNotStored) {
  // Verify that resources who only receive one update and complete are never
  // stored.
  StartResourceLoad(/*resource_id=*/0, true /* is_complete */);
  EXPECT_FALSE(HasUnfinishedResource(/*resource_id=*/0));
  EXPECT_FALSE(HasPreviousUpdateForResource(/*resource_id=*/0));

  // Load new resource and verify we don't have a previous update for the
  // resource that completed.
  StartResourceLoad(/*resource_id=*/1, true /* is_complete */);
  EXPECT_FALSE(HasUnfinishedResource(/*resource_id=*/0));
  EXPECT_FALSE(HasPreviousUpdateForResource(/*resource_id=*/0));
}
