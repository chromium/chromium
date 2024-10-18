// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ml_model/field_classification_model_encoder.h"

#include <string>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_form_test_utils.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/optimization_guide/proto/autofill_field_classification_model_metadata.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

using TokenId = FieldClassificationModelEncoder::TokenId;
using testing::ElementsAre;
using testing::TestWithParam;

namespace {
// Representation of an empty string.
constexpr TokenId kEmpty = TokenId(0);
// Representation of a token that is not in the dictionary.
constexpr TokenId kUnknown = TokenId(1);
// Representation of the token "number" (may change if the test model changes).
constexpr TokenId kNumber = TokenId(14);
// Representation of the CLS ("classification") token, which is where the
// model produces the output.
constexpr TokenId kCLS = TokenId(15);
}  // namespace

class FieldClassificationModelEncoderTest : public testing::Test {
 public:
  void SetUp() override {
    encoder_ = EncoderFromFileContents(GetTestModelMetadataPath());
  }

 protected:
  base::FilePath GetTestModelMetadataPath() {
    base::FilePath source_root_dir;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root_dir);
    return source_root_dir.AppendASCII("components")
        .AppendASCII("test")
        .AppendASCII("data")
        .AppendASCII("autofill")
        .AppendASCII("ml_model")
        .AppendASCII("autofill_model_metadata.binarypb");
  }

  FieldClassificationModelEncoder EncoderFromFileContents(
      const base::FilePath& file_path) {
    optimization_guide::proto::AutofillFieldClassificationModelMetadata
        metadata;
    std::string proto_content;
    EXPECT_TRUE(base::ReadFileToString(file_path, &proto_content));
    EXPECT_TRUE(metadata.ParseFromString(proto_content));
    return FieldClassificationModelEncoder(metadata.input_token(),
                                           metadata.encoding_parameters());
  }

  FieldClassificationModelEncoder encoder_;

 private:
  test::AutofillUnitTestEnvironment autofill_test_environment_;
};

TEST_F(FieldClassificationModelEncoderTest, TokensMappedCorrectly) {
  EXPECT_EQ(encoder_.TokenToId(u"number"), kNumber);
}

// Tests that words out of vocabulary return 1.
TEST_F(FieldClassificationModelEncoderTest, WordOutOfVocab) {
  EXPECT_EQ(encoder_.TokenToId(u"OutOfVocab"), kUnknown);
}

// Tests that empty strings return 0 for padding.
TEST_F(FieldClassificationModelEncoderTest, EmptyToken) {
  EXPECT_EQ(encoder_.TokenToId(u""), kEmpty);
}

TEST_F(FieldClassificationModelEncoderTest, InputEncodedCorrectly) {
  EXPECT_THAT(encoder_.EncodeAttribute(u"Phone 'number"),
              ElementsAre(kUnknown, kNumber, kEmpty, kEmpty, kEmpty));
}

// If a field label has more than one consecutive whitespace, they
// should all be removed without any empty strings.
TEST_F(FieldClassificationModelEncoderTest, InputHasMoreThanOneWhitespace) {
  EXPECT_EQ(encoder_.EncodeAttribute(u"Phone   &number  "),
            encoder_.EncodeAttribute(u"Phone number"));
}

TEST_F(FieldClassificationModelEncoderTest, ReplaceSpecialWithWhitespace) {
  EXPECT_EQ(encoder_.EncodeAttribute(u"Phone \u3164 number \xa0"),
            encoder_.EncodeAttribute(u"Phone number"));
}

// If a field label has more words than
// `AutofillFieldClassificationEncodingParameters::max_tokens_per_feature`,
// only the first `max_tokens_per_feature` many words should be used and the
// rest are ignored.
TEST_F(FieldClassificationModelEncoderTest,
       InputHasMoreWordsThanOutputSequenceLength) {
  EXPECT_THAT(
      encoder_.EncodeAttribute(u"City Number Phone Address Card Last Zip "),
      ElementsAre(kUnknown, kNumber, kUnknown, kUnknown, kUnknown));
}

TEST_F(FieldClassificationModelEncoderTest, InputConstructedCorrectly) {
  AutofillField field;
  field.set_label(u"Phone 'number");
  field.set_placeholder(u"Phone 'number");
  field.set_autocomplete_attribute("Phone 'number");
  EXPECT_THAT(encoder_.EncodeField(field),
              ElementsAre(
                  // CLS
                  kCLS,
                  // FEATURE_LABEL
                  kUnknown, kNumber, kEmpty, kEmpty, kEmpty,
                  // FEATURE_PLACEHOLDER
                  kUnknown, kNumber, kEmpty, kEmpty, kEmpty,
                  // FEATURE_AUTOCOMPLETE
                  kUnknown, kNumber, kEmpty, kEmpty, kEmpty));
}

TEST_F(FieldClassificationModelEncoderTest, FormEncodedCorrectly) {
  FormStructure form(test::GetFormData(
      {.fields = {
           {
               .label = u"Phone 'number",
               .placeholder = u"Phone 'number",
               .autocomplete_attribute = "Phone 'number",
           },
           {
               .label = u"City Number Phone Address Card Last Zip ",
               .placeholder = u"City Number Phone Address Card Last Zip ",
               .autocomplete_attribute =
                   "City Number Phone Address Card Last Zip ",
           }}}));
  EXPECT_THAT(
      encoder_.EncodeForm(form),
      ElementsAre(ElementsAre(
                      // CLS
                      kCLS,
                      // FEATURE_LABEL
                      kUnknown, kNumber, kEmpty, kEmpty, kEmpty,
                      // FEATURE_PLACEHOLDER
                      kUnknown, kNumber, kEmpty, kEmpty, kEmpty,
                      // FEATURE_AUTOCOMPLETE
                      kUnknown, kNumber, kEmpty, kEmpty, kEmpty),
                  ElementsAre(
                      // CLS
                      kCLS,
                      // FEATURE_LABEL
                      kUnknown, kNumber, kUnknown, kUnknown, kUnknown,
                      // FEATURE_PLACEHOLDER
                      kUnknown, kNumber, kUnknown, kUnknown, kUnknown,
                      // FEATURE_AUTOCOMPLETE
                      kUnknown, kNumber, kUnknown, kUnknown, kUnknown)));
}

struct StandardizeStringTestCase {
  bool split_on_camel_case = false;
  bool lowercase = false;
  std::string replace_chars_with_whitespace = "";
  std::string remove_chars = "";
  std::u16string input;
  std::u16string expected;
};

using StandardizeStringTest = TestWithParam<StandardizeStringTestCase>;

INSTANTIATE_TEST_SUITE_P(
    FieldClassificationModelEncoderTest,
    StandardizeStringTest,
    testing::ValuesIn<StandardizeStringTestCase>(
        {{
             .input = u"Hello World",
             .expected = u"Hello World",
         },
         {
             .split_on_camel_case = true,
             .input = u"HelloWorld thisIsACamelCaseTest ABCHello",
             .expected = u"Hello World this Is A Camel Case Test ABC Hello",
         },
         {
             // Test that casting to UTF8 does not break Japanese characters.
             // This happens in the CamelCase splitting implementation.
             .split_on_camel_case = true,
             .input = u"やまもと HelloWorld",
             .expected = u"やまもと Hello World",
         },
         {
             .lowercase = true,
             .input = u"AbCd123",
             .expected = u"abcd123",
         },
         {
             .replace_chars_with_whitespace = "_.-",
             .input = u"Chars_are-replaced.by-.whitespace",
             .expected = u"Chars are replaced by  whitespace",
         },
         {
             .remove_chars = "@!",
             .input = u"@Chars are! @removed!",
             .expected = u"Chars are removed",
         }}));

TEST_P(StandardizeStringTest, ReturnsExpectedResult) {
  const StandardizeStringTestCase& test_case = GetParam();
  optimization_guide::proto::AutofillFieldClassificationEncodingParameters
      encoding_parameters;
  encoding_parameters.set_split_on_camel_case(test_case.split_on_camel_case);
  encoding_parameters.set_lowercase(test_case.lowercase);
  encoding_parameters.set_replace_chars_with_whitespace(
      test_case.replace_chars_with_whitespace);
  encoding_parameters.set_remove_chars(test_case.remove_chars);
  FieldClassificationModelEncoder encoder({}, encoding_parameters);

  EXPECT_EQ(encoder.StandardizeString(test_case.input), test_case.expected);
}

}  // namespace autofill
