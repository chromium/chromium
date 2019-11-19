// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/field_info_manager.h"

#include <vector>

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/password_manager/core/browser/field_info_table.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::PASSWORD;
using autofill::SINGLE_USERNAME;
using autofill::UNKNOWN_TYPE;
using autofill::USERNAME;
using base::Time;

namespace password_manager {
namespace {

MATCHER_P3(FieldInfoHasData, form_signature, field_signature, field_type, "") {
  return arg.form_signature == form_signature &&
         arg.field_signature == field_signature &&
         arg.field_type == field_type && arg.create_time != base::Time();
}

class FieldInfoManagerTest : public testing::Test {
 public:
  FieldInfoManagerTest() {
    test_data_.push_back({101u, 1u, USERNAME, Time::FromTimeT(1)});
    test_data_.push_back({101u, 10u, PASSWORD, Time::FromTimeT(5)});
    test_data_.push_back({102u, 1u, SINGLE_USERNAME, Time::FromTimeT(10)});

    store_ = new MockPasswordStore;
    store_->Init(syncer::SyncableService::StartSyncFlare(), /*prefs=*/nullptr);
    EXPECT_CALL(*store_, GetAllFieldInfoImpl());
    field_info_manager_ = std::make_unique<FieldInfoManager>(store_);
    task_environment_.RunUntilIdle();
  }

  ~FieldInfoManagerTest() override { store_->ShutdownOnUIThread(); }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  scoped_refptr<MockPasswordStore> store_;
  std::vector<FieldInfo> test_data_;
  std::unique_ptr<FieldInfoManager> field_info_manager_;
};

TEST_F(FieldInfoManagerTest, AddFieldType) {
  EXPECT_EQ(UNKNOWN_TYPE, field_info_manager_->GetFieldType(101u, 1u));

  EXPECT_CALL(*store_, AddFieldInfoImpl(FieldInfoHasData(101u, 1u, PASSWORD)));
  field_info_manager_->AddFieldType(101u, 1u, PASSWORD);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(PASSWORD, field_info_manager_->GetFieldType(101u, 1u));
}

TEST_F(FieldInfoManagerTest, OnGetAllFieldInfo) {
  auto* field_info_manager_as_password_consumer =
      static_cast<PasswordStoreConsumer*>(field_info_manager_.get());
  field_info_manager_as_password_consumer->OnGetAllFieldInfo(test_data_);
  for (const FieldInfo& field_info : test_data_) {
    EXPECT_EQ(field_info.field_type,
              field_info_manager_->GetFieldType(field_info.form_signature,
                                                field_info.field_signature));
  }
  EXPECT_EQ(UNKNOWN_TYPE, field_info_manager_->GetFieldType(1234u, 1u));
}

}  // namespace
}  // namespace password_manager
