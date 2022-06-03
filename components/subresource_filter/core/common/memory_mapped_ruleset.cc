// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/common/memory_mapped_ruleset.h"

#include <utility>

#include "base/check.h"

namespace subresource_filter {

// Used for tests which want to simulate mmap failures.
static bool g_fail_memory_map_initialization_for_testing = false;

// static
scoped_refptr<MemoryMappedRuleset> MemoryMappedRuleset::CreateAndInitialize(
    base::File ruleset_file) {
  if (g_fail_memory_map_initialization_for_testing)
    return nullptr;

  auto ruleset = base::AdoptRef(new MemoryMappedRuleset());
  if (g_fail_memory_map_initialization_for_testing ||
      !ruleset->ruleset_.Initialize(std::move(ruleset_file)))
    return nullptr;
  DCHECK(ruleset->ruleset_.IsValid());
  return ruleset;
}

// static
void MemoryMappedRuleset::SetMemoryMapFailuresForTesting(bool fail) {
  g_fail_memory_map_initialization_for_testing = fail;
}

MemoryMappedRuleset::MemoryMappedRuleset() = default;
MemoryMappedRuleset::~MemoryMappedRuleset() = default;

}  // namespace subresource_filter
