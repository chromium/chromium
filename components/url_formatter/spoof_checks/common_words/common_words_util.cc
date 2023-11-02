// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_formatter/spoof_checks/common_words/common_words_util.h"

#include "net/base/lookup_string_in_fixed_set.h"

namespace url_formatter {

namespace common_words {

namespace {

#include "components/url_formatter/spoof_checks/common_words/common_words-inc.cc"

struct DafsaParams {
  const unsigned char* dafsa;
  size_t length;
};

DafsaParams g_dafsa_params{kDafsa, sizeof(kDafsa)};

}  // namespace

bool IsCommonWord(base::StringPiece word) {
  return net::LookupStringInFixedSet(g_dafsa_params.dafsa,
                                     g_dafsa_params.length, word.data(),
                                     word.size()) != net::kDafsaNotFound;
}

void SetCommonWordDAFSAForTesting(const unsigned char* dafsa, size_t length) {
  g_dafsa_params = {dafsa, length};
}

void ResetCommonWordDAFSAForTesting() {
  g_dafsa_params = {kDafsa, sizeof(kDafsa)};
}

}  // namespace common_words

}  // namespace url_formatter
