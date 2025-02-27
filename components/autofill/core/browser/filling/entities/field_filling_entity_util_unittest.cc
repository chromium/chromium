// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/filling/entities/field_filling_entity_util.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/proto/api_v1.pb.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/webdata/autofill_ai/entity_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_test_helper.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using FieldPrediction = autofill::AutofillQueryResponse::FormSuggestion::
    FieldSuggestion::FieldPrediction;

class GetFieldsFillableByAutofillAiTest : public testing::Test {
 public:
  void SetUp() override {
    testing::Test::SetUp();
    test_api(form_).PushField(
        test::CreateTestFormField("", "", "", FormControlType::kInputText));
    test_api(form_).PushField(
        test::CreateTestFormField("", "", "", FormControlType::kInputText));
  }

  void AddOrUpdateEntityInstance(EntityInstance entity) {
    edm_.AddOrUpdateEntityInstance(std::move(entity));
    helper_.WaitUntilIdle();
  }

  EntityDataManager& edm() { return edm_; }
  FormStructure& form() { return form_; }

  FieldGlobalId field(size_t i) const { return form_.fields()[i]->global_id(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kAutofillAiWithDataSchema};
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  autofill::test::AutofillUnitTestEnvironment autofill_environment_;
  AutofillWebDataServiceTestHelper helper_{std::make_unique<EntityTable>()};
  EntityDataManager edm_{helper_.autofill_webdata_service()};
  FormStructure form_{{}};
};

// If there are no Autofill AI fields, none is blocked.
TEST_F(GetFieldsFillableByAutofillAiTest, NoAutofillAiField) {
  AddOrUpdateEntityInstance(test::GetPassportEntityInstance());
  test_api(form()).SetFieldTypes({CREDIT_CARD_NAME_FULL, NAME_FULL});
  EXPECT_THAT(GetFieldsFillableByAutofillAi(form(), edm()), IsEmpty());
}

// If there is no Autofill AI entity that could fill the field, none is blocked.
TEST_F(GetFieldsFillableByAutofillAiTest, NameInFormButNotInEntity) {
  // The name is absent in the entity.
  AddOrUpdateEntityInstance(test::GetPassportEntityInstance({.name = nullptr}));
  test_api(form()).SetFieldTypes({CREDIT_CARD_NAME_FULL, NAME_FULL},
                                 {CREDIT_CARD_NAME_FULL, PASSPORT_NAME_TAG});
  EXPECT_THAT(GetFieldsFillableByAutofillAi(form(), edm()), IsEmpty());
}

// If there is a fillable AI field, it is blocked.
TEST_F(GetFieldsFillableByAutofillAiTest, FillableName) {
  AddOrUpdateEntityInstance(test::GetPassportEntityInstance());
  test_api(form()).SetFieldTypes({CREDIT_CARD_NAME_FULL, NAME_FULL},
                                 {CREDIT_CARD_NAME_FULL, PASSPORT_NAME_TAG});
  EXPECT_THAT(GetFieldsFillableByAutofillAi(form(), edm()),
              ElementsAre(field(1)));
}

// If there is a fillable AI field, it is blocked.
TEST_F(GetFieldsFillableByAutofillAiTest, FillableNumber) {
  AddOrUpdateEntityInstance(test::GetPassportEntityInstance());
  test_api(form()).SetFieldTypes({CREDIT_CARD_NAME_FULL, NAME_FULL},
                                 {CREDIT_CARD_NAME_FULL, PASSPORT_NUMBER});
  EXPECT_THAT(GetFieldsFillableByAutofillAi(form(), edm()),
              ElementsAre(field(1)));
}

TEST(GetFillValueAndTypeForEntityTest, UnobfuscatedAttributes) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAiWithDataSchema};
  AutofillField field;
  {
    FieldPrediction prediction1;
    prediction1.set_type(NAME_FIRST);
    prediction1.set_source(
        autofill::AutofillQueryResponse::FormSuggestion::FieldSuggestion::
            FieldPrediction::SOURCE_AUTOFILL_DEFAULT);
    FieldPrediction prediction2;
    prediction1.set_type(PASSPORT_NAME_TAG);
    prediction1.set_source(
        autofill::AutofillQueryResponse::FormSuggestion::FieldSuggestion::
            FieldPrediction::SOURCE_AUTOFILL_AI);
    field.set_server_predictions({prediction1, prediction2});
  }

  constexpr char16_t kName[] = u"John";
  EntityInstance passport = test::GetPassportEntityInstance({.name = kName});
  EXPECT_EQ(GetFillValueAndTypeForEntity(passport, field,
                                         mojom::ActionPersistence::kPreview)
                .first,
            kName);
  EXPECT_EQ(GetFillValueAndTypeForEntity(passport, field,
                                         mojom::ActionPersistence::kFill)
                .first,
            kName);
}

TEST(GetFillValueAndTypeForEntityTest, ObfuscatedAttributes) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAiWithDataSchema};
  AutofillField field;
  {
    FieldPrediction prediction;
    prediction.set_type(PASSPORT_NUMBER);
    prediction.set_source(
        autofill::AutofillQueryResponse::FormSuggestion::FieldSuggestion::
            FieldPrediction::SOURCE_AUTOFILL_AI);
    field.set_server_predictions({prediction});
  }

  constexpr char16_t kNumber[] = u"12";
  EntityInstance passport =
      test::GetPassportEntityInstance({.number = kNumber});
  EXPECT_EQ(GetFillValueAndTypeForEntity(passport, field,
                                         mojom::ActionPersistence::kPreview)
                .first,
            u"\u2022\u2060\u2006\u2060\u2022\u2060\u2006\u2060");
  EXPECT_EQ(GetFillValueAndTypeForEntity(passport, field,
                                         mojom::ActionPersistence::kFill)
                .first,
            kNumber);
}

TEST(GetObfuscatedAttributeValue, ObfuscateValue) {
  EntityInstance passport = test::GetPassportEntityInstance({.number = u"12"});
  base::optional_ref<const AttributeInstance> passport_number =
      passport.attribute(AttributeType(AttributeTypeName::kPassportNumber));
  ASSERT_TRUE(passport_number);
  EXPECT_EQ(GetObfuscatedAttributeValue(*passport_number),
            u"\u2022\u2060\u2006\u2060\u2022\u2060\u2006\u2060");
}

TEST(GetFillValueAndTypeForEntityTest, FillingStructuredNames) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAiWithDataSchema};
  EntityInstance passport = test::GetPassportEntityInstance();
  for (const auto& [type, expectation] :
       std::vector<std::pair<FieldType, std::u16string>>{
           {NAME_FULL, u"Pippi Långstrump"},
           {NAME_FIRST, u"Pippi"},
           {NAME_LAST, u"Långstrump"}}) {
    AutofillField field;
    FieldPrediction prediction;
    prediction.set_type(PASSPORT_NAME_TAG);
    prediction.set_source(
        autofill::AutofillQueryResponse::FormSuggestion::FieldSuggestion::
            FieldPrediction::SOURCE_AUTOFILL_AI);
    field.set_server_predictions({prediction});
    field.SetTypeTo(type);

    EXPECT_EQ(GetFillValueAndTypeForEntity(passport, field,
                                           mojom::ActionPersistence::kFill)
                  .first,
              expectation)
        << FieldTypeToStringView(type);
  }
}

}  // namespace
}  // namespace autofill
