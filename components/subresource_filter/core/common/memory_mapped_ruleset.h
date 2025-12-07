// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_MEMORY_MAPPED_RULESET_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_MEMORY_MAPPED_RULESET_H_

#include <stddef.h>
#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/files/file.h"
#include "base/files/memory_mapped_file.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/numerics/safe_conversions.h"

namespace subresource_filter {

// A reference-counted wrapper around base::MemoryMappedFile. The |ruleset_file|
// supplied in the constructor is kept memory-mapped and is safe to access until
// the last reference to this instance is dropped.
class MemoryMappedRuleset final : public base::RefCounted<MemoryMappedRuleset> {
 public:
  REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();
  static scoped_refptr<MemoryMappedRuleset> CreateAndInitialize(
      base::File ruleset_file);

  MemoryMappedRuleset(const MemoryMappedRuleset&) = delete;
  MemoryMappedRuleset& operator=(const MemoryMappedRuleset&) = delete;

  static void SetMemoryMapFailuresForTesting(bool fail);

  base::span<const uint8_t> data() const LIFETIME_BOUND {
    return ruleset_.bytes();
  }

  base::WeakPtr<MemoryMappedRuleset> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  friend class base::RefCounted<MemoryMappedRuleset>;
  MemoryMappedRuleset();
  ~MemoryMappedRuleset();

  base::MemoryMappedFile ruleset_;
  base::WeakPtrFactory<MemoryMappedRuleset> weak_ptr_factory_{this};
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_MEMORY_MAPPED_RULESET_H_
