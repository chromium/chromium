// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SHARED_HIGHLIGHTING_CORE_COMMON_SHARED_HIGHLIGHTING_FEATURES_H_
#define COMPONENTS_SHARED_HIGHLIGHTING_CORE_COMMON_SHARED_HIGHLIGHTING_FEATURES_H_

namespace base {
struct Feature;
}

namespace shared_highlighting {

// Enables link to text to be generated in advance.
extern const base::Feature kPreemptiveLinkToTextGeneration;

// If enabled, a blocklist will disable link generation on certain pages where
// the feature is unlikely to work correctly.
extern const base::Feature kSharedHighlightingUseBlocklist;

}  // namespace shared_highlighting

#endif  // COMPONENTS_SHARED_HIGHLIGHTING_CORE_COMMON_SHARED_HIGHLIGHTING_FEATURES_H_
