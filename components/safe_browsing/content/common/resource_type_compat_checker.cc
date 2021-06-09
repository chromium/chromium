// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/common/safebrowsing_constants.h"

#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"

namespace safe_browsing {

namespace {

// Verifies that safe_browsing::ResourceType and blink::mojom::ResourceType are
// kept in sync.
#define STATIC_ASSERT_ENUM(a)                                        \
  static_assert(static_cast<int>(ResourceType::a) ==                 \
                    static_cast<int>(blink::mojom::ResourceType::a), \
                "mismatching enums: ResourceType::" #a)

}  // namespace

STATIC_ASSERT_ENUM(kMainFrame);
STATIC_ASSERT_ENUM(kSubFrame);
STATIC_ASSERT_ENUM(kStylesheet);
STATIC_ASSERT_ENUM(kScript);
STATIC_ASSERT_ENUM(kImage);
STATIC_ASSERT_ENUM(kFontResource);
STATIC_ASSERT_ENUM(kSubResource);
STATIC_ASSERT_ENUM(kObject);
STATIC_ASSERT_ENUM(kMedia);
STATIC_ASSERT_ENUM(kWorker);
STATIC_ASSERT_ENUM(kSharedWorker);
STATIC_ASSERT_ENUM(kPrefetch);
STATIC_ASSERT_ENUM(kFavicon);
STATIC_ASSERT_ENUM(kXhr);
STATIC_ASSERT_ENUM(kPing);
STATIC_ASSERT_ENUM(kServiceWorker);
STATIC_ASSERT_ENUM(kCspReport);
STATIC_ASSERT_ENUM(kPluginResource);
STATIC_ASSERT_ENUM(kNavigationPreloadMainFrame);
STATIC_ASSERT_ENUM(kNavigationPreloadSubFrame);
STATIC_ASSERT_ENUM(kMaxValue);

}  // namespace safe_browsing
