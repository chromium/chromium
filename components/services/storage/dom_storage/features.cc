// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/features.h"

namespace storage {

BASE_FEATURE(kCoalesceStorageAreaCommits,
             "CoalesceStorageAreaCommits",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace storage
