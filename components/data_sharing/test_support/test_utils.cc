// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/test_support/test_utils.h"

#include "components/data_sharing/internal/data_sharing_service_impl.h"

namespace data_sharing {

GURL GetDataSharingUrl(const GroupToken& group_token) {
  return *DataSharingServiceImpl::GetDataSharingUrl(group_token);
}

}  // namespace data_sharing
