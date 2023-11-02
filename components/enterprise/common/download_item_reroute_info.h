// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_COMMON_DOWNLOAD_ITEM_REROUTE_INFO_H_
#define COMPONENTS_ENTERPRISE_COMMON_DOWNLOAD_ITEM_REROUTE_INFO_H_

#include "components/enterprise/common/proto/download_item_reroute_info.pb.h"

namespace enterprise_connectors {

bool RerouteInfosEqual(const DownloadItemRerouteInfo& info,
                       const DownloadItemRerouteInfo& other);

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_COMMON_DOWNLOAD_ITEM_REROUTE_INFO_H_
