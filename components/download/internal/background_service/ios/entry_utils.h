// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_IOS_ENTRY_UTILS_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_IOS_ENTRY_UTILS_H_

#include <map>
#include <set>
#include <vector>

#include "components/download/public/background_service/clients.h"

namespace download {

struct DownloadMetaData;
struct Entry;

namespace util {

// Creates a map of DownloadMetaData for each client. Only works on iOS.
std::map<DownloadClient, std::vector<DownloadMetaData>>
MapEntriesToMetadataForClients(const std::set<DownloadClient>& clients,
                               const std::vector<Entry*>& entries);

}  // namespace util
}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_IOS_ENTRY_UTILS_H_
