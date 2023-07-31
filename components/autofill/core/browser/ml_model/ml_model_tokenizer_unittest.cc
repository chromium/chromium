// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ml_model/ml_model_tokenizer.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class AutofillMLModelTokenizerTest : public testing::Test {
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

TEST_F(AutofillMLModelTokenizerTest, TokenizerIsInitialized) {
  auto model_tokenizer =
      AutofillMLModelTokenizer::CreateTokenizer(dictionary_path_);
  EXPECT_NE(model_tokenizer, nullptr);
}

// Initialize tokenizer with a path that does not exist.
TEST_F(AutofillMLModelTokenizerTest, WrongDictionaryPath) {
  auto missing_path = base::FilePath::FromASCII("missing");
  EXPECT_EQ(AutofillMLModelTokenizer::CreateTokenizer(missing_path), nullptr);
}

TEST_F(AutofillMLModelTokenizerTest, TokensMappedCorrectly) {
  auto model_tokenizer =
      AutofillMLModelTokenizer::CreateTokenizer(dictionary_path_);
  EXPECT_EQ(model_tokenizer->GetDictionarySize(), 11u);
  EXPECT_EQ(model_tokenizer->TokenToId(u"first"),
            AutofillMLModelTokenizer::TokenId(5));
}

// Tests that words out of vocabulary return 1.
TEST_F(AutofillMLModelTokenizerTest, WordOutOfVocab) {
  auto model_tokenizer =
      AutofillMLModelTokenizer::CreateTokenizer(dictionary_path_);
  EXPECT_EQ(model_tokenizer->TokenToId(u"address"),
            AutofillMLModelTokenizer::TokenId(1));
}

// Tests that empty strings return 0 for padding.
TEST_F(AutofillMLModelTokenizerTest, EmptyToken) {
  auto model_tokenizer =
      AutofillMLModelTokenizer::CreateTokenizer(dictionary_path_);
  EXPECT_EQ(model_tokenizer->TokenToId(u""),
            AutofillMLModelTokenizer::TokenId(0));
}

}  // namespace autofill
