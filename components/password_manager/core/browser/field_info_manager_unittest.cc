// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/field_info_manager.h"

#include <memory>

#include "base/i18n/case_conversion.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "components/password_manager/core/browser/form_parsing/password_field_prediction.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::FieldRendererId;
using autofill::FieldSignature;
using autofill::FormSignature;
using autofill::ServerFieldType;

namespace password_manager {

namespace {

const char kTestDomain[] = "https://firstdomain.com";
constexpr FormSignature kTestFormSignature(100);
constexpr FieldSignature kTestFieldSignature(200);
constexpr int kTestDriverId = 1;
constexpr FieldRendererId kTestFieldId(1);
constexpr ServerFieldType kTestFieldType = autofill::USERNAME;

const char kAnotherDomain[] = "https://seconddomain.com";
constexpr FormSignature kAnotherFormSignature(300);
constexpr FieldSignature kAnotherFieldSignature(400);
constexpr int kAnotherDriverId = 2;
constexpr FieldRendererId kAnotherFieldId(2);
constexpr ServerFieldType kAnotherFieldType = autofill::PASSWORD;

FormPredictions CreateTestPredictions(int driver_id,
                                      FormSignature form_signature,
                                      FieldSignature field_signature,
                                      FieldRendererId renderer_id,
                                      ServerFieldType type) {
  FormPredictions predictions;
  predictions.driver_id = driver_id;
  predictions.form_signature = form_signature;

  PasswordFieldPrediction field_prediction;
  field_prediction.signature = field_signature;
  field_prediction.renderer_id = renderer_id;
  field_prediction.type = type;
  predictions.fields.push_back(field_prediction);

  return predictions;
}

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
  FieldInfo info(kTestDriverId, kTestFieldId, kTestDomain, u"value",
                 /*is_likely_otp=*/false);
  manager_->AddFieldInfo(info, /*predictions=*/absl::nullopt);
  std::vector<FieldInfo> expected_info = {info};
  EXPECT_EQ(manager_->GetFieldInfo(kTestDomain), expected_info);
  EXPECT_TRUE(manager_->GetFieldInfo(kAnotherDomain).empty());

  // Check that the info is still accessible.
  task_environment_.FastForwardBy(kFieldInfoLifetime / 2);
  EXPECT_EQ(manager_->GetFieldInfo(kTestDomain), expected_info);

  // The info should not be accessible anymore
  task_environment_.FastForwardBy(kFieldInfoLifetime / 2);
  EXPECT_TRUE(manager_->GetFieldInfo(kTestDomain).empty());
}

TEST_F(FieldInfoManagerTest, InfoOverwrittenWithNewField) {
  FieldInfo info1(kTestDriverId, FieldRendererId(1), kTestDomain, u"value1",
                  /*is_likely_otp=*/false);
  manager_->AddFieldInfo(info1, /*predictions=*/absl::nullopt);
  FieldInfo info2(kTestDriverId, FieldRendererId(2), kTestDomain, u"value2",
                  /*is_likely_otp=*/false);
  manager_->AddFieldInfo(info2, /*predictions=*/absl::nullopt);

  std::vector<FieldInfo> expected_info = {info1, info2};
  EXPECT_EQ(manager_->GetFieldInfo(kTestDomain), expected_info);

  // The third info should dismiss the first one.
  FieldInfo info3(kTestDriverId, FieldRendererId(3), kTestDomain, u"value3",
                  /*is_likely_otp=*/false);
  manager_->AddFieldInfo(info3, /*predictions=*/absl::nullopt);

  expected_info = {info2, info3};
  EXPECT_EQ(manager_->GetFieldInfo(kTestDomain), expected_info);
}

TEST_F(FieldInfoManagerTest, InfoUpdatedWithNewValue) {
  FieldInfo info1(kTestDriverId, kTestFieldId, kTestDomain, u"value",
                  /*is_likely_otp=*/false);
  manager_->AddFieldInfo(info1, /*predictions=*/absl::nullopt);

  // The value should not be stored twice for the same field.
  FieldInfo info2 = info1;
  info2.value = u"new_value";
  manager_->AddFieldInfo(info2, /*predictions=*/absl::nullopt);

  std::vector<FieldInfo> expected_info = {info2};
  EXPECT_EQ(manager_->GetFieldInfo(kTestDomain), expected_info);
}

TEST_F(FieldInfoManagerTest, FieldValueLowercased) {
  std::u16string raw_value = u"VaLuE";
  FieldInfo info(kTestDriverId, kTestFieldId, kTestDomain, raw_value,
                 /*is_likely_otp=*/false);
  EXPECT_EQ(info.value, base::i18n::ToLower(raw_value));
}

TEST_F(FieldInfoManagerTest, InfoAddedWithPredictions) {
  FieldInfo info(kTestDriverId, kTestFieldId, kTestDomain, u"value",
                 /*is_likely_otp=*/false);
  FormPredictions predictions =
      CreateTestPredictions(kTestDriverId, kTestFormSignature,
                            kTestFieldSignature, kTestFieldId, kTestFieldType);
  manager_->AddFieldInfo(info, predictions);

  auto field_info_cache = manager_->GetFieldInfo(kTestDomain);
  ASSERT_EQ(field_info_cache.size(), 1u);

  EXPECT_EQ(field_info_cache[0].stored_predictions, predictions);
  EXPECT_EQ(field_info_cache[0].type, kTestFieldType);
}

TEST_F(FieldInfoManagerTest, ProcessServerPredictions) {
  FieldInfo info(kTestDriverId, kTestFieldId, kTestDomain, u"value",
                 /*is_likely_otp=*/false);
  manager_->AddFieldInfo(info, /*predictions=*/absl::nullopt);

  // Create test predictions.
  std::map<autofill::FormSignature, FormPredictions> predictions;
  FormPredictions form_prediction =
      CreateTestPredictions(kTestDriverId, kTestFormSignature,
                            kTestFieldSignature, kTestFieldId, kTestFieldType);

  // Add another field.
  PasswordFieldPrediction another_field_prediction;
  another_field_prediction.renderer_id = kAnotherFieldId;
  another_field_prediction.type = kAnotherFieldType;
  form_prediction.fields.push_back(another_field_prediction);

  predictions[kTestFormSignature] = form_prediction;

  // Add a prediction with the same field id, but different driver.
  FormPredictions different_driver_prediction = CreateTestPredictions(
      kAnotherDriverId, kAnotherFormSignature, kAnotherFieldSignature,
      kTestFieldId, kAnotherFieldType);
  predictions[kAnotherFormSignature] = different_driver_prediction;

  manager_->ProcessServerPredictions(predictions);

  auto field_info_cache = manager_->GetFieldInfo(kTestDomain);
  ASSERT_EQ(field_info_cache.size(), 1u);

  EXPECT_EQ(field_info_cache[0].stored_predictions, form_prediction);
  EXPECT_EQ(field_info_cache[0].type, kTestFieldType);
}

}  // namespace password_manager
