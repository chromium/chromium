// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/resources/returned_resource.h"

namespace viz {

ReturnedResource::ReturnedResource(ResourceId id,
                                   gpu::SyncToken sync_token,
                                   int count,
                                   bool lost)
    : id(id), sync_token(sync_token), count(count), lost(lost) {}

ReturnedResource::ReturnedResource() = default;
ReturnedResource::ReturnedResource(const ReturnedResource& other) = default;

ReturnedResource& ReturnedResource::operator=(const ReturnedResource& other) =
    default;

}  // namespace viz
