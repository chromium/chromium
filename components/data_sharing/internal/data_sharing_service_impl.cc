// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/internal/data_sharing_service_impl.h"

namespace data_sharing {

DataSharingServiceImpl::DataSharingServiceImpl() = default;

DataSharingServiceImpl::~DataSharingServiceImpl() = default;

bool DataSharingServiceImpl::IsEmptyService() {
  return false;
}

}  // namespace data_sharing
