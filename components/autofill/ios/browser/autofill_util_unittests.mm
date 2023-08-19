// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/autofill_util.h"

#import "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#import "components/autofill/core/common/field_data_manager.h"
#import "components/autofill/core/common/form_field_data.h"
#import "components/autofill/core/common/unique_ids.h"
#include "testing/platform_test.h"

using AutofillUtilTest = PlatformTest;

using autofill::ExtractIDs;
using autofill::ExtractFillingResults;
using autofill::FieldRendererId;
using base::ASCIIToUTF16;

TEST_F(AutofillUtilTest, ExtractIDs) {
  std::vector<FieldRendererId> extracted_ids;
  NSString* valid_ids = @"[\"1\",\"2\"]";
  std::vector<FieldRendererId> expected_result = {FieldRendererId(1),
                                                  FieldRendererId(2)};
  EXPECT_TRUE(ExtractIDs(valid_ids, &extracted_ids));
  EXPECT_EQ(expected_result, extracted_ids);

  extracted_ids.clear();
  NSString* empty_ids = @"[]";
  EXPECT_TRUE(ExtractIDs(empty_ids, &extracted_ids));
  EXPECT_TRUE(extracted_ids.empty());

  NSString* invalid_ids1 = @"[\"1\"\"2\"]";
  EXPECT_FALSE(ExtractIDs(invalid_ids1, &extracted_ids));
  NSString* invalid_ids2 = @"[1,2]";
  EXPECT_FALSE(ExtractIDs(invalid_ids2, &extracted_ids));
}

TEST_F(AutofillUtilTest, ExtractFillingResults) {
  std::map<uint32_t, std::u16string> extracted_results;
  NSString* valid_results = @"{\"1\":\"username\",\"2\":\"adress\"}";
  std::map<uint32_t, std::u16string> expected_result = {{1, u"username"},
                                                        {2, u"adress"}};
  EXPECT_TRUE(ExtractFillingResults(valid_results, &extracted_results));
  EXPECT_EQ(expected_result, extracted_results);

  extracted_results.clear();
  NSString* empty_results = @"{}";
  EXPECT_TRUE(ExtractFillingResults(empty_results, &extracted_results));
  EXPECT_TRUE(extracted_results.empty());

  NSString* invalid_results1 = @"{\"1\":\"username\"\"2\":\"adress\"}";
  EXPECT_FALSE(ExtractFillingResults(invalid_results1, &extracted_results));
  NSString* invalid_results2 = @"{\"1\":\"username\"\"2\":100}";
  EXPECT_FALSE(ExtractFillingResults(invalid_results2, &extracted_results));
}

// Test that the properties mask is extracted from the form field data.
TEST_F(AutofillUtilTest, ExtractFormFieldData_PropertiesMask) {
  base::Value::Dict field;
  // Set mandatory field attributes.
  field.Set("name", base::Value("email"));
  field.Set("form_control_type", base::Value("text"));

  // Set field attribute to get mask.
  field.Set("unique_renderer_id", base::Value("1"));

  const scoped_refptr<autofill::FieldDataManager> field_data_manager =
      base::MakeRefCounted<autofill::FieldDataManager>();
  // Set test field property as user typed.
  field_data_manager->UpdateFieldDataMap(
      autofill::FieldRendererId(1), u"my@mail",
      autofill::FieldPropertiesFlags::kUserTyped);

  autofill::FormFieldData field_data;
  autofill::ExtractFormFieldData(field, *field_data_manager, &field_data);

  EXPECT_EQ(u"my@mail", field_data.user_input);
  EXPECT_EQ(autofill::FieldPropertiesFlags::kUserTyped,
            field_data.properties_mask);
}
