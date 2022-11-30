// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/tools/ruleset_converter/ruleset_format.h"

#include "base/strings/string_number_conversions.h"

namespace subresource_filter {

RulesetFormat ParseFlag(const std::string& text) {
  if (text == "filter-list")
    return RulesetFormat::kFilterList;
  if (text == "proto")
    return RulesetFormat::kProto;
  if (text == "unindexed-ruleset")
    return RulesetFormat::kUnindexedRuleset;
  return RulesetFormat::kUndefined;
}

}  // namespace subresource_filter
