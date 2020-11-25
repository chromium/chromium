// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EMBEDDER_SUPPORT_ANDROID_UTIL_USER_AGENT_UTILS_H_
#define COMPONENTS_EMBEDDER_SUPPORT_ANDROID_UTIL_USER_AGENT_UTILS_H_

namespace blink {
struct UserAgentMetadata;
}

namespace content {
class WebContents;
}

namespace embedder_support {

void SetDesktopUserAgentOverride(content::WebContents* web_contents,
                                 const blink::UserAgentMetadata& metadata);

}  // namespace embedder_support

#endif  // COMPONENTS_EMBEDDER_SUPPORT_ANDROID_UTIL_USER_AGENT_UTILS_H_
