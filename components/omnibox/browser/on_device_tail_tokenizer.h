// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_ON_DEVICE_TAIL_TOKENIZER_H_
#define COMPONENTS_OMNIBOX_BROWSER_ON_DEVICE_TAIL_TOKENIZER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"

// The tokenizer performs tokenization for on device tail machine learning
// model. It basically maps raw strings to/from tokens, and tokens to/from IDs
// accepted by the ML model or vice versa based on the given vocabulary file.
// This tokenizer has not supported CJK yet.
class OnDeviceTailTokenizer {
 public:
  using TokenId = int32_t;
  using TokenIds = std::vector<TokenId>;

  // Data structure to store tokenization information.
  struct Tokenization {
    // Unambiguous token IDs. This should at least include the begin query
    // token.
    TokenIds unambiguous_ids;

    // Human-readable unambiguous part of the prefix.
    std::string unambiguous_prefix;

    // The constraint prefix for the next forward RNN step, if the last typed
    // token was ambiguous. For example, given prefix [(pa)(t)] and if the
    // trailing (t) could match multiple tokens, constraint prefix will be set
    // as "t" and only outputs matching this prefix from the next step will be
    // kept.
    std::string constraint_prefix;

    Tokenization();
    ~Tokenization();
  };

  ~OnDeviceTailTokenizer();
  OnDeviceTailTokenizer();

  // Loads the vocabulary file and initializes the tokenizer.
  bool Init(const base::FilePath& vocabulary_filepath);

  // Determines whether the instance is successfully initialized.
  bool IsReady() const;

  // Fills the Tokenization struct for the given prefix.
  void CreatePrefixTokenization(const std::string& prefix,
                                Tokenization* tokenization) const;

  // Tokenizes the previous query greedily.
  void TokenizePrevQuery(const std::string& prev_query,
                         TokenIds* prev_query_token_ids) const;

  // Resets tokens <-> IDs maps.
  void Reset();

  // Maps token to ID and vice versa.
  std::string IdToToken(const TokenId token_id) const;
  TokenId TokenToId(const std::string& token) const;

  // Returns the size of the vocabulary.
  size_t vocab_size() const { return token_to_id_.size(); }

  // Special query token related helpers.
  bool IsEndQueryTokenId(TokenId token_id) const;
  bool IsBeginQueryTokenId(TokenId token_id) const;
  TokenId GetEndQueryTokenId() const;

  // Determines if the token related to the given ID can be properly printed.
  bool IsTokenPrintable(TokenId token_id) const;

 private:
  // Determines if the given token is ambiguous.
  bool IsAmbiguousToken(const std::string& token) const;

  // Initializes the ambiguous tokens map.
  void InitAmbiguousMap();

  // Encodes the given raw string to its corresponding token and ID pairs.
  // Note we always use the longest tokens in the vocabulary first and gradually
  // switch to shorter tokens until a match for the prefix of the remaining
  // string is found. Then jump to the start of the unmatched part of the string
  // and do this again until we match all characters of the string.
  // For example, given vocabulary:
  //    [1:a], [2:b], [3:c], [4:abc], [5:ab]
  // Encoding:
  //    string:abcabc -> tokens:[abc][abc] -> IDs:[4][4]
  //    string:abcab  -> tokens:[abc][ab]  -> IDs:[4][5]
  //    string:abcaba -> tokens:[abc][ab][a] -> IDs:[4][5][1]
  //    string:cbacba -> tokens:[c][b][a][c][b][a] -> IDs:[3][2][1][3][2][1]
  void EncodeRawString(
      const std::string& raw_string,
      std::vector<std::pair<std::string, TokenId>>* token_and_ids) const;

  // Insert token and its ID to tokens <-> IDs maps.
  void InsertTokenToMaps(const std::string& token);

  // Maps for tokens <-> IDs.
  base::flat_map<std::string, TokenId> token_to_id_;
  std::vector<std::string> id_to_token_;

  // The max length of tokens in the vocabulary.
  size_t max_token_length_;

  // The list of ambiguous tokens.
  base::flat_set<std::string> ambiguous_tokens_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_ON_DEVICE_TAIL_TOKENIZER_H_
