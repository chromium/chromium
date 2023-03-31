// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/web_contents_key.h"

namespace safe_browsing {

WebContentsKey GetWebContentsKey(content::WebContents* web_contents) {
  return WebContentsKey{reinterpret_cast<uintptr_t>(web_contents)};
}

}  // namespace safe_browsing
