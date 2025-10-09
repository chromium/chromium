// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/entry.h"

namespace persistent_cache {

Entry::~Entry() = default;

size_t Entry::GetContentSize() const {
  return GetContentSpan().size();
}

}  // namespace persistent_cache
