// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/common/ruleset_dealer.h"

#include "base/check.h"
#include "base/not_fatal_until.h"
#include "components/subresource_filter/core/common/memory_mapped_ruleset.h"

namespace subresource_filter {

RulesetDealer::RulesetDealer() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

RulesetDealer::~RulesetDealer() = default;

void RulesetDealer::SetRulesetFile(base::File ruleset_file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(ruleset_file.IsValid(), base::NotFatalUntil::M129);
  ruleset_file_ = std::move(ruleset_file);
  weak_cached_ruleset_.reset();
}

bool RulesetDealer::IsRulesetFileAvailable() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ruleset_file_.IsValid();
}

scoped_refptr<const MemoryMappedRuleset> RulesetDealer::GetRuleset() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!ruleset_file_.IsValid()) {
    return nullptr;
  }

  scoped_refptr<const MemoryMappedRuleset> strong_ruleset_ref;
  if (weak_cached_ruleset_) {
    strong_ruleset_ref = weak_cached_ruleset_.get();
  } else if (scoped_refptr<MemoryMappedRuleset> ruleset =
                 MemoryMappedRuleset::CreateAndInitialize(
                     ruleset_file_.Duplicate())) {
    weak_cached_ruleset_ = ruleset->AsWeakPtr();
    strong_ruleset_ref = std::move(ruleset);
  }
  return strong_ruleset_ref;
}

base::File RulesetDealer::DuplicateRulesetFile() {
  return ruleset_file_.Duplicate();
}

}  // namespace subresource_filter
