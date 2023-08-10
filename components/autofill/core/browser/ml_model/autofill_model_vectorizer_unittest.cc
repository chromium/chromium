// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ml_model/autofill_model_vectorizer.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

using TokenId = AutofillModelVectorizer::TokenId;
using testing::ElementsAre;

class AutofillModelVectorizerTest : public testing::Test {
 public:
  void SetUp() override {
    base::FilePath source_root_dir;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &source_root_dir);
    dictionary_path_ = source_root_dir.AppendASCII("components")
                           .AppendASCII("test")
                           .AppendASCII("data")
                           .AppendASCII("autofill")
                           .AppendASCII("ml_model")
                           .AppendASCII("dictionary_test.txt");
  }

 protected:
  base::FilePath dictionary_path_;
};

TEST_F(AutofillModelVectorizerTest, VectorizerIsInitialized) {
  auto model_vectorizer =
      AutofillModelVectorizer::CreateVectorizer(dictionary_path_);
  EXPECT_NE(model_vectorizer, nullptr);
}

// Initialize vectorizer with a path that does not exist.
TEST_F(AutofillModelVectorizerTest, WrongDictionaryPath) {
  auto missing_path = base::FilePath::FromASCII("missing");
  EXPECT_EQ(AutofillModelVectorizer::CreateVectorizer(missing_path), nullptr);
}

TEST_F(AutofillModelVectorizerTest, TokensMappedCorrectly) {
  auto model_vectorizer =
      AutofillModelVectorizer::CreateVectorizer(dictionary_path_);
  EXPECT_EQ(model_vectorizer->GetDictionarySize(), 12u);
  EXPECT_EQ(model_vectorizer->TokenToId(u"first"), TokenId(8));
}

// Tests that words out of vocabulary return 1.
TEST_F(AutofillModelVectorizerTest, WordOutOfVocab) {
  auto model_vectorizer =
      AutofillModelVectorizer::CreateVectorizer(dictionary_path_);
  EXPECT_EQ(model_vectorizer->TokenToId(u"address"), TokenId(3));
}

// Tests that empty strings return 0 for padding.
TEST_F(AutofillModelVectorizerTest, EmptyToken) {
  auto model_vectorizer =
      AutofillModelVectorizer::CreateVectorizer(dictionary_path_);
  EXPECT_EQ(model_vectorizer->TokenToId(u""), TokenId(0));
}

TEST_F(AutofillModelVectorizerTest, InputVectorizedCorrectly) {
  auto model_vectorizer =
      AutofillModelVectorizer::CreateVectorizer(dictionary_path_);
  EXPECT_THAT(model_vectorizer->Vectorize(u"Phone 'number"),
              testing::ElementsAre(TokenId(11), TokenId(5), TokenId(0),
                                   TokenId(0), TokenId(0)));
}

// If a field label has more than one consecutive whitespace, they
// should all be removed without any empty strings.
TEST_F(AutofillModelVectorizerTest, InputHasMoreThanOneWhitespace) {
  auto model_vectorizer =
      AutofillModelVectorizer::CreateVectorizer(dictionary_path_);
  EXPECT_THAT(model_vectorizer->Vectorize(u"Phone   &number  "),
              testing::ElementsAre(TokenId(11), TokenId(5), TokenId(0),
                                   TokenId(0), TokenId(0)));
}

// If a field label has more words than the kOutputSequenceLength,
// only the first kOutputSequenceLength many words should be used and the
// rest are ignored.
TEST_F(AutofillModelVectorizerTest, InputHasMoreWordsThanOutputSequenceLength) {
  auto model_vectorizer =
      AutofillModelVectorizer::CreateVectorizer(dictionary_path_);
  EXPECT_THAT(
      model_vectorizer->Vectorize(u"City Number Phone Address Card Last Zip "),
      testing::ElementsAre(TokenId(6), TokenId(5), TokenId(11), TokenId(3),
                           TokenId(10)));
}

}  // namespace autofill
