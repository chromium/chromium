// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TAB_CONTEXT_HTTP_RPC_CONSTANTS_H_
#define COMPONENTS_SYNC_TAB_CONTEXT_HTTP_RPC_CONSTANTS_H_

#include "url/gurl.h"
#include "url/origin.h"

namespace sync_tab_context {

extern const char kDefaultEphemeralKeyServerUrl[];
extern const char kEphemeralKeyServerUrlSwitch[];

extern const char kDefaultTabContextAllowedOrigin[];
extern const char kTabContextAllowedOriginSwitch[];

GURL GetEphemeralKeyServerUrl();

url::Origin GetAllowedTabContextOrigin();

}  // namespace sync_tab_context

#endif  // COMPONENTS_SYNC_TAB_CONTEXT_HTTP_RPC_CONSTANTS_H_
