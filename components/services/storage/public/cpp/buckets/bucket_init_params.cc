// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/public/cpp/buckets/bucket_init_params.h"

namespace storage {

BucketInitParams::BucketInitParams(blink::StorageKey storage_key)
    : storage_key(std::move(storage_key)) {}

BucketInitParams::~BucketInitParams() = default;

BucketInitParams::BucketInitParams(const BucketInitParams&) = default;
BucketInitParams::BucketInitParams(BucketInitParams&&) noexcept = default;
BucketInitParams& BucketInitParams::operator=(const BucketInitParams&) =
    default;
BucketInitParams& BucketInitParams::operator=(BucketInitParams&&) noexcept =
    default;

}  // namespace storage
