// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/service/ui_string_overrider.h"

#include <algorithm>

#include "base/check.h"
#include "base/check_op.h"
#include "ui/base/resource/resource_bundle.h"

namespace variations {

UIStringOverrider::UIStringOverrider() = default;

UIStringOverrider::UIStringOverrider(base::span<const uint32_t> resource_hashes,
                                     base::span<const int> resource_indices)
    : resource_hashes_(resource_hashes), resource_indices_(resource_indices) {
  CHECK_EQ(resource_hashes_.size(), resource_indices_.size());
}

UIStringOverrider::UIStringOverrider(const UIStringOverrider&) = default;

UIStringOverrider::~UIStringOverrider() = default;

int UIStringOverrider::GetResourceIndex(uint32_t hash) {
  if (resource_hashes_.empty()) {
    return -1;
  }
  const auto begin = std::begin(resource_hashes_);
  const auto end = std::end(resource_hashes_);
  const auto element = std::lower_bound(begin, end, hash);
  if (element == end || *element != hash) {
    return -1;
  }
  return resource_indices_[element - begin];
}

}  // namespace variations
