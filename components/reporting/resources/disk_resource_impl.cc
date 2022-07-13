// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/resources/disk_resource_impl.h"

#include <atomic>
#include <cstdint>

#include "base/check_op.h"

namespace reporting {

DiskResourceImpl::DiskResourceImpl(uint64_t total_size) : total_(total_size) {}

DiskResourceImpl::~DiskResourceImpl() = default;

bool DiskResourceImpl::Reserve(uint64_t size) {
  uint64_t old_used = used_.fetch_add(size);
  if (old_used + size > total_) {
    used_.fetch_sub(size);
    return false;
  }
  return true;
}

void DiskResourceImpl::Discard(uint64_t size) {
  DCHECK_LE(size, used_.load());
  used_.fetch_sub(size);
}

uint64_t DiskResourceImpl::GetTotal() const {
  return total_;
}

uint64_t DiskResourceImpl::GetUsed() const {
  return used_.load();
}

void DiskResourceImpl::Test_SetTotal(uint64_t test_total) {
  total_ = test_total;
}

}  // namespace reporting
