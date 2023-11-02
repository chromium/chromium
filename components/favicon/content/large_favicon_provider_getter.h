// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_CONTENT_LARGE_FAVICON_PROVIDER_GETTER_H_
#define COMPONENTS_FAVICON_CONTENT_LARGE_FAVICON_PROVIDER_GETTER_H_

#include "base/callback.h"

namespace content {
class BrowserContext;
}

namespace favicon {

class LargeFaviconProvider;

using LargeFaviconProviderGetter =
    base::RepeatingCallback<LargeFaviconProvider*(content::BrowserContext*)>;

// Sets a callback that returns the LargeIconProvider for a given
// BrowserContext. This allows code in //components, such as LargeIconBridge, to
// obtain an implementation of LargeIconProvider even though the
// implementation's factory is unique to each embedder.
void SetLargeFaviconProviderGetter(const LargeFaviconProviderGetter& getter);
LargeFaviconProvider* GetLargeFaviconProvider(content::BrowserContext* context);

}  // namespace favicon

#endif  // COMPONENTS_FAVICON_CONTENT_LARGE_FAVICON_PROVIDER_GETTER_H_
