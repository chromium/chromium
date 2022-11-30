// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_NET_URL_TRANSLATOR_H_
#define COMPONENTS_SYNC_ENGINE_NET_URL_TRANSLATOR_H_

#include <string>

#include "url/gurl.h"

namespace syncer {

// Appends the appropriate query string to the given sync base URL.
GURL AppendSyncQueryString(const GURL& base, const std::string& client_id);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_NET_URL_TRANSLATOR_H_
