// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/resources/memory_resource_impl.h"

#include <atomic>
#include <cstdint>

#include "base/check_op.h"
#include "base/no_destructor.h"

namespace reporting {

MemoryResourceImpl::MemoryResourceImpl(uint64_t total_size)
    : total_(total_size) {}

MemoryResourceImpl::~MemoryResourceImpl() = default;

bool MemoryResourceImpl::Reserve(uint64_t size) {
  uint64_t old_used = used_.fetch_add(size);
  if (old_used + size > total_) {
    used_.fetch_sub(size);
    return false;
  }
  return true;
}

void MemoryResourceImpl::Discard(uint64_t size) {
  DCHECK_LE(size, used_.load());
  used_.fetch_sub(size);
}

uint64_t MemoryResourceImpl::GetTotal() const {
  return total_;
}

uint64_t MemoryResourceImpl::GetUsed() const {
  return used_.load();
}

void MemoryResourceImpl::Test_SetTotal(uint64_t test_total) {
  total_ = test_total;
}

}  // namespace reporting
