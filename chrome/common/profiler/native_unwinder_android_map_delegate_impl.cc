// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/profiler/native_unwinder_android_map_delegate_impl.h"

NativeUnwinderAndroidMapDelegateImpl::NativeUnwinderAndroidMapDelegateImpl(
    stack_unwinder::Module* module)
    : module_(module) {
  DCHECK(module);
}

NativeUnwinderAndroidMapDelegateImpl::~NativeUnwinderAndroidMapDelegateImpl() {
  DCHECK_EQ(reference_count_, 0u);
  DCHECK(!memory_regions_map_);
}

base::NativeUnwinderAndroidMemoryRegionsMap*
NativeUnwinderAndroidMapDelegateImpl::GetMapReference() {
  if (reference_count_ == 0) {
    DCHECK(!memory_regions_map_);
    memory_regions_map_ = module_->CreateMemoryRegionsMap();
  }
  reference_count_++;
  return memory_regions_map_.get();
}

void NativeUnwinderAndroidMapDelegateImpl::ReleaseMapReference() {
  DCHECK_GT(reference_count_, 0u);
  DCHECK(memory_regions_map_);
  if (--reference_count_ == 0) {
    memory_regions_map_.reset();
  }
}
