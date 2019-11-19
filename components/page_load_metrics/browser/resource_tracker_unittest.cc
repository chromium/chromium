// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "components/page_load_metrics/browser/resource_tracker.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "content/public/browser/global_request_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class ResourceTrackerTest : public testing::Test {
 public:
  ResourceTrackerTest() = default;

  void StartResourceLoad(int resource_id, bool is_complete = false) {
    CreateResourceUpdate(resource_id, 0 /* delta_bytes */,
                         is_complete /* is_complete */);
  }

  void AdvanceResourceLoad(int resource_id, int bytes = 0) {
    CreateResourceUpdate(resource_id, bytes /* delta_bytes */,
                         false /* is_complete */);
  }

  void CompleteResourceLoad(int resource_id) {
    CreateResourceUpdate(resource_id, 0 /* delta_bytes */,
                         true /* is_complete */);
  }

  bool HasUnfinishedResource(int resource_id) {
    return resource_tracker_.unfinished_resources().find(
               content::GlobalRequestID(process_id_, resource_id)) !=
           resource_tracker_.unfinished_resources().end();
  }

  int GetUnfinishedResourceBytes(int resource_id) {
    return resource_tracker_.unfinished_resources()
        .find(content::GlobalRequestID(process_id_, resource_id))
        ->second->delta_bytes;
  }

  bool HasPreviousUpdateForResource(int resource_id) {
    return resource_tracker_.HasPreviousUpdateForResource(
        content::GlobalRequestID(process_id_, resource_id));
  }

  int GetPreviousResourceUpdateBytes(int resource_id) {
    return resource_tracker_
        .GetPreviousUpdateForResource(
            content::GlobalRequestID(process_id_, resource_id))
        ->delta_bytes;
  }

 private:
  void CreateResourceUpdate(int request_id,
                            int64_t delta_bytes,
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
  StartResourceLoad(0 /* resource_id */);
  StartResourceLoad(1 /* resource_id */);
  StartResourceLoad(2 /* resource_id */);

  // Verify completed resources are not stored in the unfinished map.
  EXPECT_TRUE(HasUnfinishedResource(0 /* resource_id */));
  CompleteResourceLoad(0 /* resource_id */);
  EXPECT_FALSE(HasUnfinishedResource(0 /* resource_id */));

  // Verify that resources receiving multiple updates are not removed from the
  // map.
  AdvanceResourceLoad(1 /* resource_id */, 10 /* bytes */);
  AdvanceResourceLoad(1 /* resource_id */, 20 /* bytes */);
  EXPECT_TRUE(HasUnfinishedResource(1 /* resource_id */));
  CompleteResourceLoad(1 /* resource_id */);
  EXPECT_FALSE(HasUnfinishedResource(1 /* resource_id */));

  // Verify the unfinished map stores the most recent resource update.
  EXPECT_EQ(0, GetUnfinishedResourceBytes(2 /* resource_id */));
  AdvanceResourceLoad(2 /* resource_id */, 10 /* bytes */);
  EXPECT_EQ(10, GetUnfinishedResourceBytes(2 /* resource_id */));
  AdvanceResourceLoad(2 /* resource_id */, 20 /* bytes */);
  AdvanceResourceLoad(2 /* resource_id */, 30 /* bytes */);
  EXPECT_EQ(30, GetUnfinishedResourceBytes(2 /* resource_id */));
}

// Verifies that resources are added to and removed from the map
// of previous resource updates as expected.
TEST_F(ResourceTrackerTest, PreviousUpdateResourceMap) {
  StartResourceLoad(0 /* resource_id */);
  StartResourceLoad(1 /* resource_id */);
  EXPECT_FALSE(HasPreviousUpdateForResource(0 /* resource_id */));
  EXPECT_FALSE(HasPreviousUpdateForResource(1 /* resource_id */));

  AdvanceResourceLoad(1 /* resource_id */, 10 /* bytes */);
  EXPECT_TRUE(HasPreviousUpdateForResource(1 /* resource_id */));

  // Previous resource update should only be available for resources
  // who received resource updates in the previous call to
  // ResourceTracker::UpdateResourceDataUse(). resource_id "1" should not be
  // available in this case.
  AdvanceResourceLoad(0 /* resource_id */, 0 /* bytes */);
  EXPECT_FALSE(HasPreviousUpdateForResource(1 /* resource_id */));

  // The update should not be available because the load for resource_id "1" was
  // still ongoing. This should hold the data for the last update, 10 bytes.
  AdvanceResourceLoad(1 /* resource_id */, 20 /* bytes */);
  EXPECT_TRUE(HasPreviousUpdateForResource(1 /* resource_id */));
  EXPECT_EQ(10, GetPreviousResourceUpdateBytes(1 /* resource_id */));

  // Verify previous resource update is available for newly complete resources.
  CompleteResourceLoad(1 /* resource_id */);
  EXPECT_TRUE(HasPreviousUpdateForResource(1 /* resource_id */));
  EXPECT_EQ(20, GetPreviousResourceUpdateBytes(1 /* resource_id */));

  // Verify this completed resource update is removed once other resources are
  // loaded.
  CompleteResourceLoad(0 /* resource_id */);
  EXPECT_FALSE(HasPreviousUpdateForResource(1 /* resource_id */));
}

TEST_F(ResourceTrackerTest, SingleUpdateResourceNotStored) {
  // Verify that resources who only receive one update and complete are never
  // stored.
  StartResourceLoad(0 /* resource_id */, true /* is_complete */);
  EXPECT_FALSE(HasUnfinishedResource(0 /* resource_id */));
  EXPECT_FALSE(HasPreviousUpdateForResource(0 /* resource_id */));

  // Load new resource and verify we don't have a previous update for the
  // resource that completed.
  StartResourceLoad(1 /* resource_id */, true /* is_complete */);
  EXPECT_FALSE(HasUnfinishedResource(0 /* resource_id */));
  EXPECT_FALSE(HasPreviousUpdateForResource(0 /* resource_id */));
}
