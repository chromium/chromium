// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_WEB_CONTENTS_KEY_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_WEB_CONTENTS_KEY_H_

#include <cstdint>

#include "base/types/strong_alias.h"

namespace content {
class WebContents;
}  // namespace content

namespace safe_browsing {

// Key that identifies a WebContents. Derived from a WebContents* but may only
// be used for comparison, not dereferenced.
using WebContentsKey =
    base::StrongAlias<class WebContentsKeyTag, std::uintptr_t>;

// This converts the WebContents* so that it cannot be dereferenced, to avoid
// lifetime issues. Do not convert in the other direction.
WebContentsKey GetWebContentsKey(content::WebContents* web_contents);

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_WEB_CONTENTS_KEY_H_
