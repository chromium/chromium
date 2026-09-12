// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ml_model/field_classification_model_encoder_dictionary.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/protobuf/src/google/protobuf/repeated_ptr_field.h"

namespace autofill {
namespace {

using TokenId = FieldClassificationModelEncoderDictionary::TokenId;

TEST(FieldClassificationModelEncoderDictionaryTest, InitializationAndLookup) {
  google::protobuf::RepeatedPtrField<std::string> tokens;
  tokens.Add("apple");
  tokens.Add("banana");
  tokens.Add("cherry");

  FieldClassificationModelEncoderDictionary dictionary(tokens);

  // Vocabulary size should be 2 (padding + unknown) + 3 tokens = 5.
  EXPECT_EQ(dictionary.GetVocabularySize(), 5u);

  // kPaddingTokenId represents the padding token (empty string).
  EXPECT_EQ(dictionary.TokenToId(""),
            FieldClassificationModelEncoderDictionary::kPaddingTokenId);
  EXPECT_EQ(dictionary.FindTokenById(
                FieldClassificationModelEncoderDictionary::kPaddingTokenId),
            "");

  // kUnknownTokenId represents the unknown token.
  EXPECT_EQ(dictionary.TokenToId("dragonfruit"),
            FieldClassificationModelEncoderDictionary::kUnknownTokenId);
  EXPECT_EQ(dictionary.FindTokenById(
                FieldClassificationModelEncoderDictionary::kUnknownTokenId),
            "");

  // Lookups for known tokens.
  EXPECT_EQ(dictionary.TokenToId("apple"), TokenId(2));
  EXPECT_EQ(dictionary.TokenToId("banana"), TokenId(3));
  EXPECT_EQ(dictionary.TokenToId("cherry"), TokenId(4));

  // Reverse lookups.
  EXPECT_EQ(dictionary.FindTokenById(TokenId(2)), "apple");
  EXPECT_EQ(dictionary.FindTokenById(TokenId(3)), "banana");
  EXPECT_EQ(dictionary.FindTokenById(TokenId(4)), "cherry");
}

TEST(FieldClassificationModelEncoderDictionaryTest, EmptyDictionary) {
  google::protobuf::RepeatedPtrField<std::string> tokens;
  FieldClassificationModelEncoderDictionary dictionary(tokens);

  EXPECT_EQ(dictionary.GetVocabularySize(), 2u);
  EXPECT_EQ(dictionary.TokenToId(""),
            FieldClassificationModelEncoderDictionary::kPaddingTokenId);
  EXPECT_EQ(dictionary.TokenToId("anything"),
            FieldClassificationModelEncoderDictionary::kUnknownTokenId);
  EXPECT_EQ(dictionary.FindTokenById(
                FieldClassificationModelEncoderDictionary::kPaddingTokenId),
            "");
  EXPECT_EQ(dictionary.FindTokenById(
                FieldClassificationModelEncoderDictionary::kUnknownTokenId),
            "");
}

TEST(FieldClassificationModelEncoderDictionaryTest, UnsortedDictionary) {
  google::protobuf::RepeatedPtrField<std::string> tokens;
  tokens.Add("cherry");
  tokens.Add("banana");
  tokens.Add("apple");

  FieldClassificationModelEncoderDictionary dictionary(tokens);

  EXPECT_EQ(dictionary.GetVocabularySize(), 5u);

  // Token ID is based on the input order + 2 (padding + unknown).
  EXPECT_EQ(dictionary.TokenToId("cherry"), TokenId(2));
  EXPECT_EQ(dictionary.TokenToId("banana"), TokenId(3));
  EXPECT_EQ(dictionary.TokenToId("apple"), TokenId(4));

  EXPECT_EQ(dictionary.FindTokenById(TokenId(2)), "cherry");
  EXPECT_EQ(dictionary.FindTokenById(TokenId(3)), "banana");
  EXPECT_EQ(dictionary.FindTokenById(TokenId(4)), "apple");
}

}  // namespace
}  // namespace autofill
