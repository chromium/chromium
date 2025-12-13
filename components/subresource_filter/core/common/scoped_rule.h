// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_SCOPED_RULE_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_SCOPED_RULE_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"

namespace url_pattern_index::flat {
struct UrlRule;
}

namespace subresource_filter {

class MemoryMappedRuleset;

// Encapsulates a filterlist rule and its corresponding ruleset. This ensures
// that the rule remains valid as long as this `ScopedRule` object exists.
class ScopedRule {
 public:
  // Constructs an empty and invalid `ScopedRule`.
  ScopedRule();

  // Constructs a `ScopedRule` that references `rule` within `ruleset`.
  //
  // Prerequisite: `rule` is a valid rule found within the provided `ruleset`.
  ScopedRule(scoped_refptr<const MemoryMappedRuleset> ruleset,
             const url_pattern_index::flat::UrlRule* rule);

  ScopedRule(ScopedRule&& other);
  ScopedRule& operator=(ScopedRule&& other);

  ScopedRule(const ScopedRule& other);
  ScopedRule& operator=(const ScopedRule& other);

  ~ScopedRule();

  // Returns whether `rule_` is valid.
  bool IsValid() const;

  // Returns `rule_` in string form. This operation can be expensive.
  //
  // Prerequisite: `rule_` is valid.
  std::string ToString() const;

 private:
  scoped_refptr<const MemoryMappedRuleset> ruleset_;
  raw_ptr<const url_pattern_index::flat::UrlRule> rule_;
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_SCOPED_RULE_H_
