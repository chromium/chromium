// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ml_model/autofill_model_vectorizer.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/protobuf/src/google/protobuf/repeated_ptr_field.h"

namespace autofill {

using TokenId = AutofillModelVectorizer::TokenId;
using testing::ElementsAre;

class AutofillModelVectorizerTest : public testing::Test {
 public:
  void SetUp() override {
    vectorizer_ = VectorizerFromFileContents(GetTestDictionaryPath());
  }

 protected:
  AutofillModelVectorizer vectorizer_;

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

  AutofillModelVectorizer VectorizerFromFileContents(
      const base::FilePath& file) {
    std::string content;
    CHECK(base::ReadFileToString(file, &content));
    google::protobuf::RepeatedPtrField<std::string> tokens;
    for (std::string& token : base::SplitString(
             content, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
      tokens.Add(std::move(token));
    }
    return AutofillModelVectorizer(tokens);
  }
};

TEST_F(AutofillModelVectorizerTest, TokensMappedCorrectly) {
  EXPECT_EQ(vectorizer_.TokenToId(u"first"), TokenId(53));
}

// Tests that words out of vocabulary return 1.
TEST_F(AutofillModelVectorizerTest, WordOutOfVocab) {
  EXPECT_EQ(vectorizer_.TokenToId(u"OutOfVocab"), TokenId(1));
}

// Tests that empty strings return 0 for padding.
TEST_F(AutofillModelVectorizerTest, EmptyToken) {
  EXPECT_EQ(vectorizer_.TokenToId(u""), TokenId(0));
}

TEST_F(AutofillModelVectorizerTest, InputVectorizedCorrectly) {
  EXPECT_THAT(vectorizer_.Vectorize(u"Phone 'number"),
              testing::ElementsAre(TokenId(49), TokenId(40), TokenId(0),
                                   TokenId(0), TokenId(0)));
}

// If a field label has more than one consecutive whitespace, they
// should all be removed without any empty strings.
TEST_F(AutofillModelVectorizerTest, InputHasMoreThanOneWhitespace) {
  EXPECT_THAT(vectorizer_.Vectorize(u"Phone   &number  "),
              testing::ElementsAre(TokenId(49), TokenId(40), TokenId(0),
                                   TokenId(0), TokenId(0)));
}

// If a field label has more words than the kOutputSequenceLength,
// only the first kOutputSequenceLength many words should be used and the
// rest are ignored.
TEST_F(AutofillModelVectorizerTest, InputHasMoreWordsThanOutputSequenceLength) {
  EXPECT_THAT(
      vectorizer_.Vectorize(u"City Number Phone Address Card Last Zip "),
      testing::ElementsAre(TokenId(46), TokenId(40), TokenId(49), TokenId(36),
                           TokenId(43)));
}

}  // namespace autofill
