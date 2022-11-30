// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/scoped_variations_ids_provider.h"

namespace variations {

ScopedVariationsIdsProvider::ScopedVariationsIdsProvider(
    VariationsIdsProvider::Mode mode) {
  VariationsIdsProvider::CreateInstanceForTesting(mode);
}

ScopedVariationsIdsProvider::~ScopedVariationsIdsProvider() {
  VariationsIdsProvider::DestroyInstanceForTesting();
}

}  // namespace variations
