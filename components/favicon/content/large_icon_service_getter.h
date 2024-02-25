// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_CONTENT_LARGE_ICON_SERVICE_GETTER_H_
#define COMPONENTS_FAVICON_CONTENT_LARGE_ICON_SERVICE_GETTER_H_

#include "base/functional/callback.h"

namespace content {
class BrowserContext;
}

namespace favicon {

class LargeIconService;

using LargeIconServiceGetter =
    base::RepeatingCallback<LargeIconService*(content::BrowserContext*)>;

// Sets a callback that returns the LargeIconService for a given BrowserContext.
// This allows code in //components, such as LargeIconBridge, to obtain an
// implementation of LargeIconService even though the implementation's factory
// is unique to each embedder.
void SetLargeIconServiceGetter(const LargeIconServiceGetter& getter);
LargeIconService* GetLargeIconService(content::BrowserContext* context);

}  // namespace favicon

#endif  // COMPONENTS_FAVICON_CONTENT_LARGE_ICON_SERVICE_GETTER_H_
