// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/content/content_annotator/content_annotation_validator.h"

#include "base/json/json_reader.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "components/accessibility_annotator/core/accessibility_annotator_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace accessibility_annotator {

class ContentAnnotationValidatorTest : public testing::Test {
 public:
  ContentAnnotationValidatorTest() = default;
  ~ContentAnnotationValidatorTest() override = default;

 protected:
  void SetSchema(const std::string& schema) {
    feature_list_.Reset();
    feature_list_.InitAndEnableFeatureWithParameters(
        kContentAnnotator,
        {{"content_annotator_extracted_data_validation_schema", schema}});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ContentAnnotationValidatorTest, CreateReturnsNullForMalformedSchema) {
  SetSchema("invalid json");
  std::unique_ptr<ContentAnnotationValidator> validator =
      ContentAnnotationValidator::Create();
  EXPECT_EQ(validator, nullptr);
}

TEST_F(ContentAnnotationValidatorTest, IsValidatorEnabled) {
  {
    std::unique_ptr<ContentAnnotationValidator> validator =
        ContentAnnotationValidator::Create();
    ASSERT_NE(validator, nullptr);
    EXPECT_FALSE(validator->IsValidatorEnabled());
  }
  {
    SetSchema("{}");
    std::unique_ptr<ContentAnnotationValidator> validator =
        ContentAnnotationValidator::Create();
    ASSERT_NE(validator, nullptr);
    EXPECT_FALSE(validator->IsValidatorEnabled());
  }
  {
    SetSchema(R"({"cat": {"field": "string"}})");
    std::unique_ptr<ContentAnnotationValidator> validator =
        ContentAnnotationValidator::Create();
    ASSERT_NE(validator, nullptr);
    EXPECT_TRUE(validator->IsValidatorEnabled());
  }
}

TEST_F(ContentAnnotationValidatorTest, ValidateFailsWithNoSchema) {
  std::unique_ptr<ContentAnnotationValidator> validator =
      ContentAnnotationValidator::Create();
  ASSERT_NE(validator, nullptr);
  std::string data = R"({"key": "value"})";

  // Validate returns nullopt if schema is empty.
  std::optional<std::string> result = validator->Validate(data);
  EXPECT_FALSE(result.has_value());
}

TEST_F(ContentAnnotationValidatorTest, ValidateRejectsInvalidJson) {
  SetSchema(R"({"cat": {"field": "string"}})");
  std::unique_ptr<ContentAnnotationValidator> validator =
      ContentAnnotationValidator::Create();
  ASSERT_NE(validator, nullptr);
  std::string data = "invalid json";

  std::optional<std::string> result = validator->Validate(data);
  EXPECT_FALSE(result.has_value());
}

TEST_F(ContentAnnotationValidatorTest, ValidateRejectsHtmlChars) {
  SetSchema(R"({"cat": {"field": "string"}})");
  std::unique_ptr<ContentAnnotationValidator> validator =
      ContentAnnotationValidator::Create();
  ASSERT_NE(validator, nullptr);
  std::string data = R"({"key": "some <script>alert(1)</script> value"})";

  std::optional<std::string> result = validator->Validate(data);
  EXPECT_FALSE(result.has_value());
}

TEST_F(ContentAnnotationValidatorTest, ValidateRejectsControlChars) {
  SetSchema(R"({"cat": {"field": "string"}})");
  std::unique_ptr<ContentAnnotationValidator> validator =
      ContentAnnotationValidator::Create();
  ASSERT_NE(validator, nullptr);

  std::string data = "{\"key\": \"some \x01 value\"}";

  std::optional<std::string> result = validator->Validate(data);
  EXPECT_FALSE(result.has_value());
}

TEST_F(ContentAnnotationValidatorTest, ValidateAllowsWhitespace) {
  SetSchema(R"({"cat": {"field": "string"}})");
  std::unique_ptr<ContentAnnotationValidator> validator =
      ContentAnnotationValidator::Create();
  ASSERT_NE(validator, nullptr);

  std::string data = "{\"key\": \t\n\r\"some value\"}";

  std::optional<std::string> result = validator->Validate(data);
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), data);
}

}  // namespace accessibility_annotator
