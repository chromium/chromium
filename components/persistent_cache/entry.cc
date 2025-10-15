// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/entry.h"

namespace persistent_cache {

Entry::~Entry() = default;

size_t Entry::GetContentSize() const {
  return GetContentSpan().size();
}

size_t Entry::CopyContentTo(base::span<uint8_t> content) const {
  size_t resulting_size = std::min(content.size(), GetContentSpan().size());
  content.subspan(size_t(0), resulting_size)
      .copy_from(GetContentSpan().subspan(size_t(0), resulting_size));
  return resulting_size;
}

}  // namespace persistent_cache
