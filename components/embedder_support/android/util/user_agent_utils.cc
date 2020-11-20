// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/android/util/user_agent_utils.h"

#include "components/version_info/version_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/user_agent.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"

namespace embedder_support {

void SetDesktopUserAgentOverride(content::WebContents* web_contents,
                                 const blink::UserAgentMetadata& metadata) {
  const char kLinuxInfoStr[] = "X11; Linux x86_64";
  std::string product = version_info::GetProductNameAndVersionForUserAgent();

  blink::UserAgentOverride spoofed_ua;
  spoofed_ua.ua_string_override =
      content::BuildUserAgentFromOSAndProduct(kLinuxInfoStr, product);
  spoofed_ua.ua_metadata_override = metadata;
  spoofed_ua.ua_metadata_override->platform = "Linux";
  spoofed_ua.ua_metadata_override->platform_version =
      std::string();  // match content::GetOSVersion(false) on Linux
  spoofed_ua.ua_metadata_override->architecture = "x86";
  spoofed_ua.ua_metadata_override->model = std::string();
  spoofed_ua.ua_metadata_override->mobile = false;

  web_contents->SetUserAgentOverride(spoofed_ua, false);
}

}  // namespace embedder_support
