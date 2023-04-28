// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/public/cpp/buckets/constants.h"

namespace storage {

// Leading underscores are disallowed for user-specified buckets. This name is
// intentionally chosen to be non-overlapping with the set of allowed
// user-specified bucket names.
const char kDefaultBucketName[] = "_default";

}  // namespace storage
