// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ml_model/autofill_model_encoder.h"

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

using TokenId = AutofillModelEncoder::TokenId;
using testing::ElementsAre;

class AutofillModelEncoderTest : public testing::Test {
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

  AutofillModelEncoder EncoderFromFileContents(
      const base::FilePath& file_path) {
    optimization_guide::proto::AutofillFieldClassificationModelMetadata
        metadata;
    std::string proto_content;
    EXPECT_TRUE(base::ReadFileToString(file_path, &proto_content));
    EXPECT_TRUE(metadata.ParseFromString(proto_content));
    return AutofillModelEncoder(metadata.input_token());
  }

  AutofillModelEncoder encoder_;

 private:
  test::AutofillUnitTestEnvironment autofill_test_environment_;
};

TEST_F(AutofillModelEncoderTest, TokensMappedCorrectly) {
  EXPECT_EQ(encoder_.TokenToId(u"number"), TokenId(22));
}

// Tests that words out of vocabulary return 1.
TEST_F(AutofillModelEncoderTest, WordOutOfVocab) {
  EXPECT_EQ(encoder_.TokenToId(u"OutOfVocab"), TokenId(1));
}

// Tests that empty strings return 0 for padding.
TEST_F(AutofillModelEncoderTest, EmptyToken) {
  EXPECT_EQ(encoder_.TokenToId(u""), TokenId(0));
}

TEST_F(AutofillModelEncoderTest, InputEncodedCorrectly) {
  EXPECT_THAT(
      encoder_.TokenizeAttribute(u"Phone 'number"),
      ElementsAre(TokenId(1), TokenId(22), TokenId(0), TokenId(0), TokenId(0)));
}

// If a field label has more than one consecutive whitespace, they
// should all be removed without any empty strings.
TEST_F(AutofillModelEncoderTest, InputHasMoreThanOneWhitespace) {
  EXPECT_THAT(
      encoder_.TokenizeAttribute(u"Phone   &number  "),
      ElementsAre(TokenId(1), TokenId(22), TokenId(0), TokenId(0), TokenId(0)));
}

TEST_F(AutofillModelEncoderTest, ReplaceSpecialWithWhitespace) {
  EXPECT_THAT(
      encoder_.TokenizeAttribute(u"Phone \u3164 number \xa0"),
      ElementsAre(TokenId(1), TokenId(22), TokenId(0), TokenId(0), TokenId(0)));
}

// If a field label has more words than the kAttributeOutputSequenceLength,
// only the first kOutputSequenceLength many words should be used and the
// rest are ignored.
TEST_F(AutofillModelEncoderTest, InputHasMoreWordsThanOutputSequenceLength) {
  EXPECT_THAT(
      encoder_.TokenizeAttribute(u"City Number Phone Address Card Last Zip "),
      ElementsAre(TokenId(1), TokenId(22), TokenId(1), TokenId(1), TokenId(1)));
}

TEST_F(AutofillModelEncoderTest, AttributeEncodedCorrectly) {
  EXPECT_THAT(encoder_.EncodeAttribute(
                  u"Phone \u3164 number \xa0",
                  AutofillModelEncoder::FieldAttributeIdentifier::kPlaceholder),
              ElementsAre(TokenId(46), TokenId(1), TokenId(22), TokenId(0),
                          TokenId(0), TokenId(0)));
}

TEST_F(AutofillModelEncoderTest, InputConstructedCorrectly) {
  AutofillField field;
  field.set_label(u"Phone 'number");
  field.set_placeholder(u"Phone 'number");
  field.set_autocomplete_attribute("Phone 'number");
  EXPECT_THAT(
      encoder_.EncodeField(field),
      ElementsAre(TokenId(45), TokenId(1), TokenId(22), TokenId(0), TokenId(0),
                  TokenId(0), TokenId(47), TokenId(1), TokenId(22), TokenId(0),
                  TokenId(0), TokenId(0), TokenId(46), TokenId(1), TokenId(22),
                  TokenId(0), TokenId(0), TokenId(0)));
}

TEST_F(AutofillModelEncoderTest, FormEncodedCorrectly) {
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
      ElementsAre(ElementsAre(TokenId(45), TokenId(1), TokenId(22), TokenId(0),
                              TokenId(0), TokenId(0), TokenId(47), TokenId(1),
                              TokenId(22), TokenId(0), TokenId(0), TokenId(0),
                              TokenId(46), TokenId(1), TokenId(22), TokenId(0),
                              TokenId(0), TokenId(0)),
                  ElementsAre(TokenId(45), TokenId(1), TokenId(22), TokenId(1),
                              TokenId(1), TokenId(1), TokenId(47), TokenId(1),
                              TokenId(22), TokenId(1), TokenId(1), TokenId(1),
                              TokenId(46), TokenId(1), TokenId(22), TokenId(1),
                              TokenId(1), TokenId(1))));
}

}  // namespace autofill
