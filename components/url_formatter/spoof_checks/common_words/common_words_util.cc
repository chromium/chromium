// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_formatter/spoof_checks/common_words/common_words_util.h"

#include <cstdint>
#include <optional>
#include <string_view>

#include "base/containers/span.h"
#include "components/url_formatter/spoof_checks/common_words/common_words-inc.cc"
#include "net/base/lookup_string_in_fixed_set.h"

namespace url_formatter::common_words {

namespace {

base::span<const uint8_t> g_dafsa_params = kDafsa;

}  // namespace

bool IsCommonWord(std::string_view word) {
  return net::LookupStringInFixedSet(g_dafsa_params, word).has_value();
}

void SetCommonWordDAFSAForTesting(base::span<const uint8_t> dafsa) {
  g_dafsa_params = dafsa;
}

void ResetCommonWordDAFSAForTesting() {
  g_dafsa_params = kDafsa;
}

}  // namespace url_formatter::common_words
