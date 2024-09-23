// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_RULESET_DEALER_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_RULESET_DEALER_H_

#include "base/files/file.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"

namespace subresource_filter {

class MemoryMappedRuleset;

// Memory maps the subresource filtering ruleset file, and makes it available to
// all call sites of GetRuleset() within the current process.
//
// Although most OS'es will take care of memory mapping pages of the ruleset
// only on-demand, and of swapping out pages eagerly when they are no longer
// used, this class has extra logic to make sure memory is conserved as much as
// possible, and syscall overhead is minimized:
//
//   * The ruleset is only memory mapped on-demand the first time GetRuleset()
//     is called, and unmapped once the last caller drops its reference to it.
//
//   * If the ruleset is used by multiple call sites within the same process,
//     they will use the same, cached, MemoryMappedRuleset instance, and will
//     not call mmap() multiple times.
//
class RulesetDealer {
 public:
  RulesetDealer();

  RulesetDealer(const RulesetDealer&) = delete;
  RulesetDealer& operator=(const RulesetDealer&) = delete;

  virtual ~RulesetDealer();

  // Sets the |ruleset_file| to memory map and distribute from now on.
  virtual void SetRulesetFile(base::File ruleset_file);

  // Returns whether |this| dealer has a ruleset file to read from. Does not
  // interact with the file system.
  bool IsRulesetFileAvailable() const;

  // Returns the set |ruleset_file|. Normally, the same instance is used by all
  // call sites in a given process. That instance is mapped lazily and unmapped
  // eagerly as soon as the last reference to it is dropped.
  virtual scoped_refptr<const MemoryMappedRuleset> GetRuleset();

  // For testing only.
  bool has_cached_ruleset() const { return !!weak_cached_ruleset_.get(); }

  // Duplicates the ruleset file. This is used to pass the file to another
  // thread.
  base::File DuplicateRulesetFile();

 protected:
  SEQUENCE_CHECKER(sequence_checker_);

 private:
  friend class SubresourceFilterRulesetDealerTest;

  base::File ruleset_file_;
  base::WeakPtr<MemoryMappedRuleset> weak_cached_ruleset_;
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_RULESET_DEALER_H_
