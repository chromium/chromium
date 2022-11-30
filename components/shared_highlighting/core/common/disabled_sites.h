// Copyright 2020 The Chromium Authors
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

// Returns true if given url supports link generation in a child iframe.
bool SupportsLinkGenerationInIframe(GURL main_frame_url);

}  // namespace shared_highlighting

#endif  // COMPONENTS_SHARED_HIGHLIGHTING_CORE_COMMON_DISABLED_SITES_H_
