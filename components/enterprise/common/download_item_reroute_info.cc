// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/common/download_item_reroute_info.h"

namespace enterprise_connectors {

bool RerouteInfosEqual(const DownloadItemRerouteInfo& info,
                       const DownloadItemRerouteInfo& other) {
  return info.GetTypeName() == other.GetTypeName() &&
         info.IsInitialized() == other.IsInitialized() &&
         (info.IsInitialized()
              ? (info.SerializeAsString() == other.SerializeAsString())
              : true);
}

}  // namespace enterprise_connectors
