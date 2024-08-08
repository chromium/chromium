// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <string_view>

#include "ash/public/cpp/desk_template.h"
#include "base/json/json_reader.h"
#include "base/test/task_environment.h"
#include "components/account_id/account_id.h"
#include "components/desks_storage/core/desk_sync_bridge.h"
#include "components/desks_storage/core/desk_template_conversion.h"
#include "components/desks_storage/core/desk_test_util.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/sync/protocol/workspace_desk_specifics.pb.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace desks_storage {

namespace {

base::Value PerformPolicyRoundtrip(const base::Value& expected,
                                   DeskSyncBridge* bridge,
                                   apps::AppRegistryCache* cache) {
  auto policy_dt = desk_template_conversion::ParseDeskTemplateFromBaseValue(
      expected, ash::DeskTemplateSource::kPolicy);

  EXPECT_TRUE(policy_dt.has_value());

  sync_pb::WorkspaceDeskSpecifics proto_desk =
      desk_template_conversion::ToSyncProto(policy_dt.value().get(), cache);

  // Convert back to original format.
  return desk_template_conversion::SerializeDeskTemplateAsBaseValue(
      desk_template_conversion::FromSyncProto(proto_desk).get(), cache);
}

}  // namespace

class DeskTemplateSemanticsTest : public testing::TestWithParam<std::string> {
 public:
  DeskTemplateSemanticsTest(const DeskTemplateSemanticsTest&) = delete;
  DeskTemplateSemanticsTest& operator=(const DeskTemplateSemanticsTest&) =
      delete;

 protected:
  DeskTemplateSemanticsTest()
      : cache_(std::make_unique<apps::AppRegistryCache>()),
        account_id_(AccountId::FromUserEmail("test@gmail.com")),
        store_(syncer::DataTypeStoreTestUtil::CreateInMemoryStoreForTest()) {}

  // testing::test.
  void SetUp() override {
    ON_CALL(mock_processor_, IsTrackingMetadata())
        .WillByDefault(testing::Return(true));
    bridge_ = std::make_unique<DeskSyncBridge>(
        mock_processor_.CreateForwardingProcessor(),
        syncer::DataTypeStoreTestUtil::FactoryForForwardingStore(store_.get()),
        account_id_);
    desk_test_util::PopulateAppRegistryCache(account_id_, cache_.get());
  }

  DeskSyncBridge* bridge() { return bridge_.get(); }

  apps::AppRegistryCache* app_cache() { return cache_.get(); }

 private:
  // In memory data type store needs to be able to post tasks.
  base::test::TaskEnvironment task_environment_;
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> mock_processor_;
  std::unique_ptr<apps::AppRegistryCache> cache_;
  AccountId account_id_;
  std::unique_ptr<DeskSyncBridge> bridge_;
  std::unique_ptr<syncer::DataTypeStore> store_;
};

TEST_P(DeskTemplateSemanticsTest, PolicyTemplateSemanticallyEquivalentToProto) {
  auto expected_json = base::JSONReader::ReadAndReturnValueWithError(
      std::string_view(GetParam()));

  EXPECT_TRUE(expected_json.has_value());
  EXPECT_TRUE(expected_json->is_dict());

  base::Value got_json =
      PerformPolicyRoundtrip(*expected_json, bridge(), app_cache());

  EXPECT_EQ(*expected_json, got_json);
}

INSTANTIATE_TEST_SUITE_P(
    PolicySemanticsEquivalencyTest,
    DeskTemplateSemanticsTest,
    ::testing::Values(
        desk_test_util::kValidPolicyTemplateBrowser,
        desk_test_util::kValidPolicyTemplateBrowserMinimized,
        desk_test_util::kValidPolicyTemplateChromeAndProgressive,
        desk_test_util::kValidPolicyTemplateChromeForFloatingWorkspace));

}  // namespace desks_storage
