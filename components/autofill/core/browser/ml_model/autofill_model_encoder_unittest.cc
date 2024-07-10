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
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/protobuf/src/google/protobuf/repeated_ptr_field.h"

namespace autofill {

using TokenId = AutofillModelEncoder::TokenId;
using testing::ElementsAre;

class AutofillModelEncoderTest : public testing::Test {
 public:
  void SetUp() override {
    encoder_ = EncoderFromFileContents(GetTestDictionaryPath());
  }

 protected:
  AutofillModelEncoder encoder_;

 private:
  base::FilePath GetTestDictionaryPath() {
    base::FilePath source_root_dir;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root_dir);
    return source_root_dir.AppendASCII("components")
        .AppendASCII("test")
        .AppendASCII("data")
        .AppendASCII("autofill")
        .AppendASCII("ml_model")
        .AppendASCII("br_overfitted_dictionary_test.txt");
  }

  AutofillModelEncoder EncoderFromFileContents(const base::FilePath& file) {
    std::string content;
    CHECK(base::ReadFileToString(file, &content));
    google::protobuf::RepeatedPtrField<std::string> tokens;
    for (std::string& token : base::SplitString(
             content, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
      tokens.Add(std::move(token));
    }
    return AutofillModelEncoder(tokens);
  }

  test::AutofillUnitTestEnvironment autofill_test_environment_;
};

TEST_F(AutofillModelEncoderTest, TokensMappedCorrectly) {
  EXPECT_EQ(encoder_.TokenToId(u"first"), TokenId(53));
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
  EXPECT_THAT(encoder_.EncodeAttribute(u"Phone 'number"),
              ElementsAre(TokenId(49), TokenId(40), TokenId(0), TokenId(0),
                          TokenId(0)));
}

// If a field label has more than one consecutive whitespace, they
// should all be removed without any empty strings.
TEST_F(AutofillModelEncoderTest, InputHasMoreThanOneWhitespace) {
  EXPECT_THAT(encoder_.EncodeAttribute(u"Phone   &number  "),
              ElementsAre(TokenId(49), TokenId(40), TokenId(0), TokenId(0),
                          TokenId(0)));
}

// If a field label has more words than the kAttributeOutputSequenceLength,
// only the first kOutputSequenceLength many words should be used and the
// rest are ignored.
TEST_F(AutofillModelEncoderTest, InputHasMoreWordsThanOutputSequenceLength) {
  EXPECT_THAT(
      encoder_.EncodeAttribute(u"City Number Phone Address Card Last Zip "),
      ElementsAre(TokenId(46), TokenId(40), TokenId(49), TokenId(36),
                  TokenId(43)));
}

TEST_F(AutofillModelEncoderTest, InputConstructedCorrectly) {
  AutofillField field;
  field.set_label(u"Phone 'number");
  EXPECT_THAT(encoder_.EncodeField(field),
              ElementsAre(TokenId(49), TokenId(40), TokenId(0), TokenId(0),
                          TokenId(0)));
}

TEST_F(AutofillModelEncoderTest, FormEncodedCorrectly) {
  FormStructure form(test::GetFormData(
      {.fields = {{.label = u"Phone 'number"},
                  {.label = u"City Number Phone Address Card Last Zip "}}}));
  EXPECT_THAT(encoder_.EncodeForm(form),
              ElementsAre(ElementsAre(TokenId(49), TokenId(40), TokenId(0),
                                      TokenId(0), TokenId(0)),
                          ElementsAre(TokenId(46), TokenId(40), TokenId(49),
                                      TokenId(36), TokenId(43))));
}

}  // namespace autofill
