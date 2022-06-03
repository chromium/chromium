// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_matcher/string_pattern.h"

#include <tuple>
#include <utility>

#include "base/check_op.h"

namespace url_matcher {

StringPattern::StringPattern(std::string pattern, StringPattern::ID id)
    : pattern_(std::move(pattern)), id_(id) {
  DCHECK_NE(kInvalidId, id_);
}

StringPattern::~StringPattern() {}

StringPattern::StringPattern(StringPattern&&) = default;
StringPattern& StringPattern::operator=(StringPattern&&) = default;

bool StringPattern::operator<(const StringPattern& rhs) const {
  return std::tie(id_, pattern_) < std::tie(rhs.id_, rhs.pattern_);
}

}  // namespace url_matcher
