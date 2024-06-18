// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_formatter/spoof_checks/common_words/common_words_util.h"

#include <cstdint>
#include <string_view>

#include "base/containers/span.h"
#include "net/base/lookup_string_in_fixed_set.h"

namespace url_formatter {

namespace common_words {

namespace {

#include "components/url_formatter/spoof_checks/common_words/common_words-inc.cc"

base::span<const uint8_t> g_dafsa_params = kDafsa;

}  // namespace

bool IsCommonWord(std::string_view word) {
  return net::LookupStringInFixedSet(g_dafsa_params, word.data(),
                                     word.size()) != net::kDafsaNotFound;
}

void SetCommonWordDAFSAForTesting(base::span<const uint8_t> dafsa) {
  g_dafsa_params = dafsa;
}

void ResetCommonWordDAFSAForTesting() {
  g_dafsa_params = kDafsa;
}

}  // namespace common_words

}  // namespace url_formatter
