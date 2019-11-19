// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_SERVER_FORBIDDEN_CHECK_REQUEST_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_SERVER_FORBIDDEN_CHECK_REQUEST_H_

#include <vector>

#include "components/offline_pages/core/prefetch/prefetch_network_request_factory.h"
#include "components/offline_pages/core/prefetch/prefetch_service.h"
#include "components/prefs/pref_service.h"

namespace offline_pages {

// Offline prefetching isn't available in some countries due to copyright
// laws. GeneratePageBundle requests from these countries are rejected by
// Offline Pages Service with a "forbidden" error. A client that has received
// the "forbidden" response can check periodically whether it is still
// forbidden, i.e. whether the user has started making requests from an
// allowed country. This is for checking whether the client is forbidden
// by making a GeneratePageBundle request with no URLs.
void CheckIfEnabledByServer(PrefService* pref_service,
                            PrefetchService* prefetch_service);
}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_SERVER_FORBIDDEN_CHECK_REQUEST_H_
