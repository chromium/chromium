// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/field_info_manager.h"

#include <memory>

#include "base/i18n/case_conversion.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::FieldRendererId;
using autofill::FieldSignature;
using autofill::FormSignature;

namespace password_manager {

namespace {

const char kFirstDomain[] = "https://firstdomain.com";
const char kSecondDomain[] = "https://seconddomain.com";

}  // namespace

class FieldInfoManagerTest : public testing::Test {
 public:
  void SetUp() override {
    manager_ = std::make_unique<FieldInfoManager>(
        task_environment_.GetMainThreadTaskRunner());
  }

 protected:
  std::unique_ptr<FieldInfoManager> manager_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(FieldInfoManagerTest, InfoAddedRetrievedAndExpired) {
  FieldInfo info(/*driver_id=*/1, FieldRendererId(1), kFirstDomain, u"value");
  manager_->AddFieldInfo(info);
  std::vector<FieldInfo> expected_info = {info};
  EXPECT_EQ(manager_->GetFieldInfo(kFirstDomain), expected_info);
  EXPECT_TRUE(manager_->GetFieldInfo(kSecondDomain).empty());

  // Check that the info is still accessible.
  task_environment_.FastForwardBy(kFieldInfoLifetime / 2);
  EXPECT_EQ(manager_->GetFieldInfo(kFirstDomain), expected_info);

  // The info should not be accessible anymore
  task_environment_.FastForwardBy(kFieldInfoLifetime / 2);
  EXPECT_TRUE(manager_->GetFieldInfo(kFirstDomain).empty());
}

TEST_F(FieldInfoManagerTest, InfoOverwrittenWithNewField) {
  FieldInfo info1(/*driver_id=*/1, FieldRendererId(1), kFirstDomain, u"value1");
  manager_->AddFieldInfo(info1);
  FieldInfo info2(/*driver_id=*/2, FieldRendererId(2), kFirstDomain, u"value2");
  manager_->AddFieldInfo(info2);

  std::vector<FieldInfo> expected_info = {info1, info2};
  EXPECT_EQ(manager_->GetFieldInfo(kFirstDomain), expected_info);

  // The third info should dismiss the first one.
  FieldInfo info3(/*driver_id=*/3, FieldRendererId(3), kFirstDomain, u"value3");
  manager_->AddFieldInfo(info3);

  expected_info = {info2, info3};
  EXPECT_EQ(manager_->GetFieldInfo(kFirstDomain), expected_info);
}

TEST_F(FieldInfoManagerTest, InfoUpdatedWithNewValue) {
  FieldInfo info1(/*driver_id=*/1, FieldRendererId(1), kFirstDomain, u"value");
  manager_->AddFieldInfo(info1);

  // The value should not be stored twice for the same field.
  FieldInfo info2 = info1;
  info2.value = u"new_value";
  manager_->AddFieldInfo(info2);

  std::vector<FieldInfo> expected_info = {info2};
  EXPECT_EQ(manager_->GetFieldInfo(kFirstDomain), expected_info);
}

TEST_F(FieldInfoManagerTest, FieldValueLowercased) {
  std::u16string raw_value = u"VaLuE";
  FieldInfo info(/*driver_id=*/1, FieldRendererId(1), kFirstDomain, raw_value);
  EXPECT_EQ(info.value, base::i18n::ToLower(raw_value));
}

}  // namespace password_manager
