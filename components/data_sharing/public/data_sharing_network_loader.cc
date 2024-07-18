// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/public/data_sharing_network_loader.h"

namespace data_sharing {

DataSharingNetworkLoader::LoadResult::LoadResult() = default;

DataSharingNetworkLoader::LoadResult::LoadResult(std::string result_bytes,
                                                 NetworkLoaderStatus status)
    : result_bytes(std::move(result_bytes)), status(status) {}

DataSharingNetworkLoader::LoadResult::~LoadResult() = default;

}  // namespace data_sharing
