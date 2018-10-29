// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/service/ui_string_overrider.h"

#include <algorithm>

#include "base/logging.h"
#include "ui/base/resource/resource_bundle.h"

namespace variations {

UIStringOverrider::UIStringOverrider()
    : resource_hashes_(nullptr),
      resource_indices_(nullptr),
      num_resources_(0) {}

UIStringOverrider::UIStringOverrider(const uint32_t* resource_hashes,
                                     const int* resource_indices,
                                     size_t num_resources)
    : resource_hashes_(resource_hashes),
      resource_indices_(resource_indices),
      num_resources_(num_resources) {
  DCHECK(!num_resources || resource_hashes_);
  DCHECK(!num_resources || resource_indices_);
}

UIStringOverrider::~UIStringOverrider() {}

int UIStringOverrider::GetResourceIndex(uint32_t hash) {
  if (!num_resources_)
    return -1;
  const uint32_t* end = resource_hashes_ + num_resources_;
  const uint32_t* element = std::lower_bound(resource_hashes_, end, hash);
  if (element == end || *element != hash)
    return -1;
  return resource_indices_[element - resource_hashes_];
}

}  // namespace variations
