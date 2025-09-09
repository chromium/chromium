// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/scoped_variations_ids_provider.h"

namespace variations::test {

ScopedVariationsIdsProvider::ScopedVariationsIdsProvider(
    VariationsIdsProvider::Mode mode)
    : previous_instance_(VariationsIdsProvider::CreateInstanceForTesting(mode)),
      current_instance_(VariationsIdsProvider::GetInstance()) {
}

ScopedVariationsIdsProvider::~ScopedVariationsIdsProvider() {
  CHECK_EQ(current_instance_, VariationsIdsProvider::GetInstance());
  // Release our pointer to the current instance before destroying it, as
  // `DestroyInstanceForTesting()` will delete the global instance and reset
  // the global instance pointer to the previous instance.
  current_instance_ = nullptr;
  VariationsIdsProvider::DestroyInstanceForTesting(previous_instance_);
}

VariationsIdsProvider* ScopedVariationsIdsProvider::operator->() {
  CHECK_EQ(current_instance_, VariationsIdsProvider::GetInstance());
  return current_instance_;
}

VariationsIdsProvider& ScopedVariationsIdsProvider::operator*() {
  CHECK_EQ(current_instance_, VariationsIdsProvider::GetInstance());
  return *current_instance_;
}

}  // namespace variations::test
