// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/internal/tab_group_sync_personal_collaboration_data_handler.h"

#include <memory>
#include <vector>

#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "components/data_sharing/test_support/mock_personal_collaboration_data_service.h"
#include "components/saved_tab_groups/internal/saved_tab_group_model.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/test_support/saved_tab_group_test_utils.h"
#include "components/sync/base/collaboration_id.h"
#include "components/sync/protocol/shared_tab_group_data_specifics.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tab_groups {
namespace {

class TabGroupSyncPersonalCollaborationDataHandlerTest : public testing::Test {
 public:
  TabGroupSyncPersonalCollaborationDataHandlerTest() = default;
  ~TabGroupSyncPersonalCollaborationDataHandlerTest() override = default;

 protected:
  void SetUp() override {
    model_ = std::make_unique<SavedTabGroupModel>();
    handler_ = std::make_unique<TabGroupSyncPersonalCollaborationDataHandler>(
        model_.get(), &personal_collaboration_data_service_);
  }

  void TearDown() override { handler_.reset(); }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<SavedTabGroupModel> model_;
  testing::NiceMock<data_sharing::MockPersonalCollaborationDataService>
      personal_collaboration_data_service_;
  std::unique_ptr<TabGroupSyncPersonalCollaborationDataHandler> handler_;
};

TEST_F(TabGroupSyncPersonalCollaborationDataHandlerTest,
       ApplyUpdatesFromSpecifics) {
  // 1. Create a group and a tab in the model.
  SavedTabGroup group(test::CreateTestSavedTabGroup());
  group.SetCollaborationId(syncer::CollaborationId("test_id"));
  SavedTabGroupTab tab(GURL("https://www.google.com"), u"Google",
                       group.saved_guid(),
                       /*position=*/0);
  model_->AddedLocally(group);
  model_->AddTabToGroupLocally(group.saved_guid(), tab);
  ASSERT_FALSE(model_->Get(group.saved_guid())->position().has_value());

  // 2. Create specifics for the group and tab.
  sync_pb::SharedTabGroupAccountDataSpecifics group_specifics;
  group_specifics.set_guid(group.saved_guid().AsLowercaseString());
  auto* group_details = group_specifics.mutable_shared_tab_group_details();
  group_details->set_pinned_position(1);

  sync_pb::SharedTabGroupAccountDataSpecifics tab_specifics;
  tab_specifics.set_guid(tab.saved_tab_guid().AsLowercaseString());
  auto* tab_details = tab_specifics.mutable_shared_tab_details();
  tab_details->set_shared_tab_group_guid(
      group.saved_guid().AsLowercaseString());
  base::Time new_last_seen_time = base::Time::Now();
  tab_details->set_last_seen_timestamp_windows_epoch(
      new_last_seen_time.ToDeltaSinceWindowsEpoch().InMicroseconds());

  // 3. Mock the service to return the specifics.
  std::vector<const sync_pb::SharedTabGroupAccountDataSpecifics*>
      all_specifics = {&group_specifics, &tab_specifics};
  EXPECT_CALL(personal_collaboration_data_service_, GetAllSpecifics)
      .WillOnce(testing::Return(all_specifics));

  // 4. Call OnInitialized.
  handler_->OnInitialized();

  // 5. Verify the group and tab are updated.
  const SavedTabGroup* updated_group = model_->Get(group.saved_guid());
  ASSERT_TRUE(updated_group->is_shared_tab_group());
  EXPECT_TRUE(updated_group->position().has_value());
  EXPECT_EQ(updated_group->position().value(), 1u);

  const SavedTabGroupTab* updated_tab =
      updated_group->GetTab(tab.saved_tab_guid());
  EXPECT_EQ(updated_tab->last_seen_time(), new_last_seen_time);
}

}  // namespace
}  // namespace tab_groups
