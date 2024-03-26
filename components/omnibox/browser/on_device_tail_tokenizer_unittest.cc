// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/on_device_tail_tokenizer.h"

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAreArray;

class OnDeviceTailTokenizerTest : public ::testing::Test {
 public:
  OnDeviceTailTokenizerTest() { tokenizer_.Reset(); }

 protected:
  void InitializeTokenizer(const std::string filename) {
    base::FilePath file_path;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &file_path);
    file_path =
        file_path.AppendASCII("components/test/data/omnibox/" + filename);

    ASSERT_TRUE(base::PathExists(file_path));
    tokenizer_.Init(file_path);
    EXPECT_TRUE(tokenizer_.IsReady());
  }

  OnDeviceTailTokenizer tokenizer_;
};

TEST_F(OnDeviceTailTokenizerTest, IsTokenPrintable) {
  InitializeTokenizer("vocab_test.txt");

  EXPECT_TRUE(tokenizer_.IsTokenPrintable(33));
  EXPECT_TRUE(tokenizer_.IsTokenPrintable(260));
  EXPECT_FALSE(tokenizer_.IsTokenPrintable(1));
  EXPECT_FALSE(tokenizer_.IsTokenPrintable(257));
  EXPECT_FALSE(tokenizer_.IsTokenPrintable(600));
}

TEST_F(OnDeviceTailTokenizerTest, CreatePrefixTokenization) {
  {
    SCOPED_TRACE("Test for ASCII vocab #1.");
    InitializeTokenizer("vocab_test.txt");
    OnDeviceTailTokenizer::Tokenization tokenization;
    // Expect tokens ["n", "j", " ", "do", "c"].
    // See OnDeviceTailTokenizer::EncodeRawString for details and simplified
    // examples about how ID sequences are determined.
    tokenizer_.CreatePrefixTokenization("nj doc", &tokenization);
    EXPECT_THAT(tokenization.unambiguous_ids,
                testing::ElementsAreArray({257, 110, 106, 32, 297}));
    EXPECT_EQ("c", tokenization.constraint_prefix);
    EXPECT_EQ("nj do", tokenization.unambiguous_prefix);
  }

  {
    SCOPED_TRACE("Test for ASCII vocab #2.");
    InitializeTokenizer("vocab_test.txt");
    OnDeviceTailTokenizer::Tokenization tokenization;
    // Expect tokens ["re", "mi", "t", "ly", " ", "log", "in"].
    tokenizer_.CreatePrefixTokenization("remitly login", &tokenization);
    EXPECT_THAT(tokenization.unambiguous_ids,
                testing::ElementsAreArray({257, 414, 366, 116, 363, 32, 521}));
    EXPECT_EQ("in", tokenization.constraint_prefix);
    EXPECT_EQ("remitly log", tokenization.unambiguous_prefix);
  }

  {
    SCOPED_TRACE("Test for ASCII vocab #3.");
    InitializeTokenizer("vocab_test.txt");
    OnDeviceTailTokenizer::Tokenization tokenization;
    // Expect tokens
    // ["us", " ", "pa", "ss", "po", "rt", " ", "ap", "pl", "ica", "tio", "n"]
    tokenizer_.CreatePrefixTokenization("us passport application",
                                        &tokenization);
    EXPECT_THAT(tokenization.unambiguous_ids,
                testing::ElementsAreArray({257, 456, 32, 402, 434, 407, 424, 32,
                                           270, 406, 507, 549}));
    EXPECT_EQ("n", tokenization.constraint_prefix);
    EXPECT_EQ("us passport applicatio", tokenization.unambiguous_prefix);
  }

  {
    SCOPED_TRACE("Test for i18n languages.");
    InitializeTokenizer("vocab_i18n_test.txt");
    OnDeviceTailTokenizer::Tokenization tokenization;
    // Expect tokens
    // ["us", "ल्", "वावि", "てる",  "a", "वा"]
    tokenizer_.CreatePrefixTokenization("usल्वाविてるaवा", &tokenization);
    EXPECT_THAT(tokenization.unambiguous_ids,
                testing::ElementsAreArray({257, 259, 260, 263, 264, 97}));
    EXPECT_EQ("वा", tokenization.constraint_prefix);
    EXPECT_EQ("usल्वाविてるa", tokenization.unambiguous_prefix);
  }
}

TEST_F(OnDeviceTailTokenizerTest, TokenizePrevQuery) {
  {
    SCOPED_TRACE("Test for ASCII vocab #1.");
    InitializeTokenizer("vocab_test.txt");
    OnDeviceTailTokenizer::TokenIds token_ids;
    tokenizer_.TokenizePrevQuery("facebook", &token_ids);

    // Expect tokens: ["fa", "ce", "bo", "ok"]
    EXPECT_EQ(4, (int)token_ids.size());
    EXPECT_THAT(token_ids, ElementsAreArray({317, 285, 281, 390}));
    EXPECT_EQ("fa", tokenizer_.IdToToken(token_ids[0]));
  }

  {
    SCOPED_TRACE("Test for ASCII vocab #2.");
    InitializeTokenizer("vocab_test.txt");
    OnDeviceTailTokenizer::TokenIds token_ids;
    tokenizer_.TokenizePrevQuery("matching gym outfits", &token_ids);

    // Expect tokens:
    // ["ma", "t", "chi", "ng", " ", "g", "y", "m", " ", "out", "fi", "ts"]
    EXPECT_EQ(12, (int)token_ids.size());
    EXPECT_THAT(token_ids, ElementsAreArray({364, 116, 488, 375, 32, 103, 121,
                                             109, 32, 533, 320, 443}));
    EXPECT_EQ("ma", tokenizer_.IdToToken(token_ids[0]));
  }

  {
    SCOPED_TRACE("Test for i18n languages.");
    InitializeTokenizer("vocab_i18n_test.txt");
    OnDeviceTailTokenizer::TokenIds token_ids;
    tokenizer_.TokenizePrevQuery("usल्वाविてるaवा", &token_ids);

    // Expect tokens:
    // ["us", "ल्", "वावि", "てる",  "a", "वा"]
    EXPECT_EQ(6, (int)token_ids.size());
    EXPECT_THAT(token_ids, ElementsAreArray({259, 260, 263, 264, 97, 261}));
    EXPECT_EQ("us", tokenizer_.IdToToken(token_ids[0]));
  }
}
