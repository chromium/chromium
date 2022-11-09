// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/contact_info_sync_bridge.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/contact_info_sync_util.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/mock_autofill_webdata_backend.h"
#include "components/sync/model/client_tag_based_model_type_processor.h"
#include "components/sync/model/data_batch.h"
#include "components/webdata/common/web_database.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using testing::ElementsAre;
using testing::UnorderedElementsAre;

constexpr char kGUID1[] = "00000000-0000-0000-0000-000000000001";
constexpr char kGUID2[] = "00000000-0000-0000-0000-000000000002";
constexpr char kInvalidGUID[] = "1234";

// Extracts all `ContactInfoSpecifics` from `batch`, converts them into
// `AutofillProfile`s and returns the result.
// Note that for consistency with the `AUTOFILL_PROFILE` sync type,
// `CONTACT_INFO` uses the same local `AutofillProfile` model.
std::vector<AutofillProfile> ExtractAutofillProfilesFromDataBatch(
    std::unique_ptr<syncer::DataBatch> batch) {
  std::vector<AutofillProfile> profiles;
  while (batch->HasNext()) {
    const syncer::KeyAndData& data_pair = batch->Next();
    profiles.push_back(*CreateAutofillProfileFromContactInfoSpecifics(
        data_pair.second->specifics.contact_info()));
  }
  return profiles;
}

AutofillProfile TestProfile(base::StringPiece guid) {
  return AutofillProfile(std::string(guid), /*origin=*/"",
                         AutofillProfile::Source::kAccount);
}

}  // namespace

class ContactInfoSyncBridgeTest : public testing::Test {
 public:
  // Creates the `bridge()` and mocks its `AutofillTable`.
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_.AddTable(&table_);
    db_.Init(temp_dir_.GetPath().AppendASCII("SyncTestWebDatabase"));
    ON_CALL(backend_, GetDatabase()).WillByDefault(testing::Return(&db_));

    auto processor = std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
        syncer::CONTACT_INFO, /*dump_stack=*/base::DoNothing());
    bridge_ = std::make_unique<ContactInfoSyncBridge>(std::move(processor),
                                                      &backend_);
  }

  // Adds multiple `profiles` the `bridge()`'s AutofillTable.
  void AddAutofillProfilesToTable(
      const std::vector<AutofillProfile>& profiles) {
    for (const auto& profile : profiles) {
      table_.AddAutofillProfile(profile);
    }
  }

  // Synchronously gets all data from the `bridge()`.
  std::vector<AutofillProfile> GetAllDataFromBridge() {
    std::vector<AutofillProfile> profiles;
    base::RunLoop loop;
    bridge().GetAllDataForDebugging(base::BindLambdaForTesting(
        [&](std::unique_ptr<syncer::DataBatch> batch) {
          profiles = ExtractAutofillProfilesFromDataBatch(std::move(batch));
          loop.Quit();
        }));
    loop.Run();
    return profiles;
  }

  ContactInfoSyncBridge& bridge() { return *bridge_; }

 private:
  base::ScopedTempDir temp_dir_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  testing::NiceMock<MockAutofillWebDataBackend> backend_;
  AutofillTable table_;
  WebDatabase db_;
  std::unique_ptr<ContactInfoSyncBridge> bridge_;
};

TEST_F(ContactInfoSyncBridgeTest, GetStorageKey) {
  // Valid case.
  std::unique_ptr<syncer::EntityData> entity =
      CreateContactInfoEntityDataFromAutofillProfile(TestProfile(kGUID1));
  EXPECT_EQ(kGUID1, bridge().GetStorageKey(*entity));
  // Invalid case.
  entity->specifics.mutable_contact_info()->set_guid(kInvalidGUID);
  EXPECT_TRUE(bridge().GetStorageKey(*entity).empty());
}

// Tests that `GetData()` returns all local profiles of matching GUID.
TEST_F(ContactInfoSyncBridgeTest, GetData) {
  const AutofillProfile profile1 = TestProfile(kGUID1);
  const AutofillProfile profile2 = TestProfile(kGUID2);
  AddAutofillProfilesToTable({profile1, profile2});

  // Synchronously get data the data of `kGUID1`.
  std::vector<AutofillProfile> profiles;
  base::RunLoop loop;
  bridge().GetData(
      {kGUID1},
      base::BindLambdaForTesting([&](std::unique_ptr<syncer::DataBatch> batch) {
        profiles = ExtractAutofillProfilesFromDataBatch(std::move(batch));
        loop.Quit();
      }));
  loop.Run();
  EXPECT_THAT(profiles, ElementsAre(profile1));
}

// Tests that `GetAllDataForDebugging()` returns all local data.
TEST_F(ContactInfoSyncBridgeTest, GetAllDataForDebugging) {
  const AutofillProfile profile1 = TestProfile(kGUID1);
  const AutofillProfile profile2 = TestProfile(kGUID2);
  AddAutofillProfilesToTable({profile1, profile2});
  EXPECT_THAT(GetAllDataFromBridge(), UnorderedElementsAre(profile1, profile2));
}

}  // namespace autofill
