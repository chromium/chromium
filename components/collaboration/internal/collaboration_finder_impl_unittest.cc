// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/collaboration_finder_impl.h"

#include <memory>

#include "base/test/gmock_callback_support.h"
#include "components/data_sharing/public/group_data.h"
#include "components/data_sharing/test_support/mock_data_sharing_service.h"
#include "components/saved_tab_groups/public/collaboration_finder.h"
#include "components/sync/base/collaboration_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Eq;
using testing::Invoke;
using testing::Return;

namespace collaboration {
namespace {

class MockCollaborationFinderClient
    : public tab_groups::CollaborationFinder::Client {
 public:
  MockCollaborationFinderClient() = default;
  ~MockCollaborationFinderClient() override = default;

  MOCK_METHOD(void, OnCollaborationAvailable, (const syncer::CollaborationId&));
};

class CollaborationFinderImplTest : public testing::Test {
 protected:
  CollaborationFinderImplTest() = default;
  ~CollaborationFinderImplTest() override = default;

  void CreateFinder() {
    collaboration_finder_ =
        std::make_unique<CollaborationFinderImpl>(&data_sharing_service_);
  }

  data_sharing::MockDataSharingService data_sharing_service_;
  std::unique_ptr<CollaborationFinderImpl> collaboration_finder_;
  MockCollaborationFinderClient client_;
};

TEST_F(CollaborationFinderImplTest, ObservesDataSharingService) {
  EXPECT_CALL(data_sharing_service_, AddObserver(_));
  CreateFinder();
  EXPECT_CALL(data_sharing_service_, RemoveObserver(_));
  collaboration_finder_.reset();
}

TEST_F(CollaborationFinderImplTest, IsCollaborationAvailable) {
  CreateFinder();
  collaboration_finder_->SetClient(&client_);

  const data_sharing::GroupId group_id("test_group_id");
  data_sharing::GroupData group_data;
  group_data.group_token.group_id = group_id;

  ON_CALL(data_sharing_service_, ReadGroup(group_id))
      .WillByDefault(testing::Return(std::make_optional<>(group_data)));
  EXPECT_EQ(true, collaboration_finder_->IsCollaborationAvailable(
                      syncer::CollaborationId(*group_id)));

  ON_CALL(data_sharing_service_, ReadGroup(group_id))
      .WillByDefault(testing::Return(std::nullopt));
  EXPECT_EQ(false, collaboration_finder_->IsCollaborationAvailable(
                       syncer::CollaborationId(*group_id)));
}

TEST_F(CollaborationFinderImplTest, OnGroupAdded) {
  CreateFinder();
  collaboration_finder_->SetClient(&client_);

  const data_sharing::GroupId group_id("test_group_id");
  data_sharing::GroupData group_data;
  group_data.group_token.group_id = group_id;

  EXPECT_CALL(client_,
              OnCollaborationAvailable(Eq(syncer::CollaborationId(*group_id))))
      .Times(1);
  collaboration_finder_->OnGroupAdded(group_data, base::Time());
}

}  // namespace
}  // namespace collaboration
