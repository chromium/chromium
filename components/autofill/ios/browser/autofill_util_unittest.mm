// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/autofill_util.h"

#import <optional>
#import <variant>

#import "base/memory/scoped_refptr.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/string_util.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "base/values.h"
#import "components/autofill/core/common/field_data_manager.h"
#import "components/autofill/core/common/form_data.h"
#import "components/autofill/core/common/form_field_data.h"
#import "components/autofill/core/common/unique_ids.h"
#import "components/autofill/ios/common/features.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"
#import "url/origin.h"

namespace autofill {

namespace {

using AutofillUtilTest = PlatformTest;

using ::base::ASCIIToUTF16;
using ::testing::AllOf;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::ResultOf;
using ::testing::SizeIs;
using ::testing::VariantWith;

TEST_F(AutofillUtilTest, ExtractFormData_FullUrl) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kAutofillExtractFullUrlOnIOs);

  base::DictValue form_dict;
  form_dict.Set("name", "form_name");
  form_dict.Set("origin", "https://example.com");
  form_dict.Set("host_frame", "11111111111111111111111111111111");
  base::ListValue fields;
  base::DictValue field;
  field.Set("name", "field_name");
  field.Set("form_control_type", "text");
  fields.Append(std::move(field));
  form_dict.Set("fields", std::move(fields));

  GURL main_frame_url("https://example.com");
  url::Origin form_frame_origin =
      url::Origin::Create(GURL("https://example.com"));
  GURL form_frame_url("https://user:pass@example.com/foo?bar=baz");
  auto field_data_manager = base::MakeRefCounted<FieldDataManager>();
  std::string frame_id = "11111111111111111111111111111111";

  base::expected<FormData, ExtractFormDataFailure> result = ExtractFormData(
      form_dict, /*formNameFilter=*/std::nullopt, main_frame_url,
      form_frame_origin, form_frame_url, *field_data_manager, frame_id);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value().full_url(), GURL("https://example.com/foo?bar=baz"));
}

TEST_F(AutofillUtilTest, ExtractFormData_NoFullUrlWhenDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kAutofillExtractFullUrlOnIOs);

  base::DictValue form_dict;
  form_dict.Set("name", "form_name");
  form_dict.Set("origin", "https://example.com");
  form_dict.Set("host_frame", "11111111111111111111111111111111");
  base::ListValue fields;
  base::DictValue field;
  field.Set("name", "field_name");
  field.Set("form_control_type", "text");
  fields.Append(std::move(field));
  form_dict.Set("fields", std::move(fields));

  GURL main_frame_url("https://example.com");
  url::Origin form_frame_origin =
      url::Origin::Create(GURL("https://example.com"));
  GURL form_frame_url("https://example.com/foo?bar=baz");
  auto field_data_manager = base::MakeRefCounted<FieldDataManager>();
  std::string frame_id = "11111111111111111111111111111111";

  base::expected<FormData, ExtractFormDataFailure> result = ExtractFormData(
      form_dict, /*formNameFilter=*/std::nullopt, main_frame_url,
      form_frame_origin, form_frame_url, *field_data_manager, frame_id);

  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result.value().full_url().is_empty());
}

TEST_F(AutofillUtilTest, ExtractIDs) {
  NSString* valid_ids = @"[\"1\",\"2\"]";
  std::set<FieldRendererId> expected_result = {FieldRendererId(1),
                                               FieldRendererId(2)};
  std::optional<std::set<FieldRendererId>> extracted_ids =
      ExtractIDs<FieldRendererId>(valid_ids);
  EXPECT_TRUE(extracted_ids);
  EXPECT_EQ(expected_result, *extracted_ids);

  NSString* empty_ids = @"[]";
  extracted_ids = ExtractIDs<FieldRendererId>(empty_ids);
  EXPECT_TRUE(extracted_ids);
  EXPECT_TRUE(extracted_ids.value().empty());

  NSString* invalid_ids1 = @"[\"1\"\"2\"]";
  EXPECT_FALSE(ExtractIDs<FieldRendererId>(invalid_ids1));
  NSString* invalid_ids2 = @"[1,2]";
  EXPECT_FALSE(ExtractIDs<FieldRendererId>(invalid_ids2));
  NSString* too_big_id = @"[\"111222333444\"]";
  EXPECT_FALSE(ExtractIDs<FieldRendererId>(too_big_id));
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

// Tests that out-of-range max_length values fall back to the default so that
// the extracted value never exceeds FormFieldData::kDefaultMaxLength.
TEST_F(AutofillUtilTest, ExtractFormFieldData_MaxLength) {
  const scoped_refptr<autofill::FieldDataManager> field_data_manager =
      base::MakeRefCounted<autofill::FieldDataManager>();

  const struct {
    std::optional<double> max_length;
    uint64_t expected;
  } kCases[] = {
      {std::nullopt, autofill::FormFieldData::kDefaultMaxLength},
      {-1.0, autofill::FormFieldData::kDefaultMaxLength},
      {0.0, 0u},
      {10.0, 10u},
      {1e15, autofill::FormFieldData::kDefaultMaxLength},
  };
  for (const auto& test : kCases) {
    SCOPED_TRACE(testing::Message()
                 << "Testing max_length: "
                 << (test.max_length ? base::NumberToString(*test.max_length)
                                     : "std::nullopt"));

    base::DictValue field;
    // Set mandatory field attributes.
    field.Set("name", base::Value("name"));
    field.Set("form_control_type", base::Value("text"));
    if (test.max_length) {
      field.Set("max_length", base::Value(*test.max_length));
    }

    autofill::FormFieldData field_data;
    ASSERT_TRUE(autofill::ExtractFormFieldData(field, *field_data_manager,
                                               &field_data));
    EXPECT_EQ(test.expected, field_data.max_length());
    EXPECT_LE(field_data.max_length(),
              autofill::FormFieldData::kDefaultMaxLength);
  }
}

// Test that the properties mask is extracted from the form field data.
TEST_F(AutofillUtilTest, ExtractFormFieldData_PropertiesMask) {
  base::DictValue field;
  // Set mandatory field attributes.
  field.Set("name", base::Value("email"));
  field.Set("form_control_type", base::Value("text"));

  // Set field attribute to get mask.
  field.Set("renderer_id", base::Value("1"));

  const scoped_refptr<FieldDataManager> field_data_manager =
      base::MakeRefCounted<FieldDataManager>();
  // Set test field property as user typed.
  field_data_manager->UpdateFieldDataMap(FieldRendererId(1), u"my@mail",
                                         FieldPropertiesFlags::kUserTyped);

  FormFieldData field_data;
  ExtractFormFieldData(field, *field_data_manager, &field_data);

  EXPECT_EQ(u"my@mail", field_data.user_input());
  EXPECT_EQ(FieldPropertiesFlags::kUserTyped, field_data.properties_mask());
}

// Tests various aspects of converting hex IDs equivalent to those generated by
// JavaScript into UnguessableTokens.
TEST_F(AutofillUtilTest, DeserializeTokens) {
  // Should work with a 32-character (128-bit) hex string. Also test that
  // hex conversion is robust to upper/lower case.
  std::optional<base::UnguessableToken> token =
      DeserializeJavaScriptFrameId("0123456789abcdef0123456789ABCDEF");
  ASSERT_TRUE(token.has_value());
  EXPECT_EQ("0123456789abcdef0123456789abcdef",
            base::ToLowerASCII(token->ToString()));

  // Should fail if the string has the wrong length
  token = DeserializeJavaScriptFrameId(std::string(4, '1'));
  EXPECT_FALSE(token.has_value());
  token = DeserializeJavaScriptFrameId(std::string(34, 'f'));
  EXPECT_FALSE(token.has_value());

  // Should fail if the string isn't hex
  token = DeserializeJavaScriptFrameId(std::string(32, '?'));
  EXPECT_FALSE(token.has_value());
}

// Test that the child frames is extracted from the form field data.
TEST_F(AutofillUtilTest, ExtractRemoteFrameToken) {
  base::DictValue wellformed1;
  wellformed1.Set("token", base::Value("beefbeefbeefbeefcafecafecafecafe"));
  wellformed1.Set("predecessor", base::Value(64));

  EXPECT_THAT(
      ExtractRemoteFrameTokenForTest(wellformed1),
      Optional(AllOf(Field(&FrameTokenWithPredecessor::token,
                           VariantWith<RemoteFrameToken>(ResultOf(
                               [](const RemoteFrameToken& t) {
                                 return base::ToLowerASCII(t.ToString());
                               },
                               "beefbeefbeefbeefcafecafecafecafe"))),
                     Field(&FrameTokenWithPredecessor::predecessor, 64))));

  base::DictValue malformed1;
  malformed1.Set("garbage", base::Value("garbage"));
  EXPECT_EQ(ExtractRemoteFrameTokenForTest(malformed1), std::nullopt);

  base::DictValue malformed2;
  malformed2.Set("token", base::Value("garbage"));
  EXPECT_EQ(ExtractRemoteFrameTokenForTest(malformed2), std::nullopt);

  base::DictValue malformed3;
  malformed3.Set("token", base::Value("beefbeefbeefbeefcafecafecafecafe"));
  malformed3.Set("predecessor", base::Value("garbage"));
  EXPECT_EQ(ExtractRemoteFrameTokenForTest(malformed3), std::nullopt);

  // Test that -1 is the only negative number supported for `predecessor`.
  base::DictValue wellformed2 = wellformed1.Clone();
  wellformed2.Set("predecessor", base::Value(-1));
  EXPECT_NE(ExtractRemoteFrameTokenForTest(wellformed2), std::nullopt);

  base::DictValue malformed4 = wellformed1.Clone();
  malformed4.Set("predecessor", base::Value(-5));
  EXPECT_EQ(ExtractRemoteFrameTokenForTest(malformed4), std::nullopt);
}

// Tests that ExtractChildFrames() only accepts predecessors that in ascending
// order.
TEST_F(AutofillUtilTest, ExtractChildFrames_PredecessorsMustBeSorted) {
  auto create_child = [](std::string token, int predecessor) {
    base::DictValue child;
    child.Set("token", base::Value(std::move(token)));
    child.Set("predecessor", base::Value(predecessor));
    return child;
  };
  auto create_children = [](auto&&... children) {
    base::ListValue list;
    (list.Append(std::move(children)), ...);
    return list;
  };

  base::DictValue form;
  EXPECT_THAT(ExtractChildFramesForTest(form), IsEmpty());

  form.Set("child_frames", base::ListValue());
  EXPECT_THAT(ExtractChildFramesForTest(form), IsEmpty());

  form.Set("child_frames", create_children());
  EXPECT_THAT(ExtractChildFramesForTest(form), IsEmpty());

  form.Set("child_frames", create_children(create_child(
                               "aeefbeefbeefbeefcafecafecafecafe", 12)));
  EXPECT_THAT(ExtractChildFramesForTest(form), SizeIs(1));

  form.Set(
      "child_frames",
      create_children(create_child("aeefbeefbeefbeefcafecafecafecafe", 12),
                      create_child("beefbeefbeefbeefcafecafecafecafe", 23)));
  EXPECT_THAT(ExtractChildFramesForTest(form), SizeIs(2));

  form.Set(
      "child_frames",
      create_children(create_child("aeefbeefbeefbeefcafecafecafecafe", -1),
                      create_child("beefbeefbeefbeefcafecafecafecafe", 12),
                      create_child("ceefbeefbeefbeefcafecafecafecafe", 23)));
  EXPECT_THAT(ExtractChildFramesForTest(form), SizeIs(3));

  form.Set(
      "child_frames",
      create_children(create_child("aeefbeefbeefbeefcafecafecafecafe", 99),
                      create_child("beefbeefbeefbeefcafecafecafecafe", 1)));
  EXPECT_THAT(ExtractChildFramesForTest(form), IsEmpty());
}

}  // namespace

}  // namespace autofill
