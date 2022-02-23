// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SHARED_HIGHLIGHTING_CORE_COMMON_DISABLED_SITES_H_
#define COMPONENTS_SHARED_HIGHLIGHTING_CORE_COMMON_DISABLED_SITES_H_

#include "url/gurl.h"

namespace shared_highlighting {

// Returns true iff Link to Text menu options should be enabled on this page.
// Uses a blocklist to identify certain sites where personalized or dynamic
// content make it unlikely that a generated URL will actually work when
// shared.
bool ShouldOfferLinkToText(const GURL& url);

// Returns true if given url supports link generation in iframe.
// This will only be called if the url is an iframe.
bool SupportsLinkGenerationInIframe(GURL url);

}  // namespace shared_highlighting

#endif  // COMPONENTS_SHARED_HIGHLIGHTING_CORE_COMMON_DISABLED_SITES_H_
