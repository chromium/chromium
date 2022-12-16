// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EMBEDDER_SUPPORT_ANDROID_UTIL_CDN_UTILS_H_
#define COMPONENTS_EMBEDDER_SUPPORT_ANDROID_UTIL_CDN_UTILS_H_

#include "base/feature_list.h"

class GURL;

namespace content {
class Page;
}

namespace embedder_support {

// This should be called from content::WebContentsObserver::PrimaryPageChanged
// to get a publisher url for the committed navigation, else an empty GURL().
GURL GetPublisherURL(content::Page& page);

}  // namespace embedder_support

#endif  //  COMPONENTS_EMBEDDER_SUPPORT_ANDROID_UTIL_CDN_UTILS_H_
