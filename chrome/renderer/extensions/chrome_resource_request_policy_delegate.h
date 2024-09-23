// Copyright 2024 the Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_CHROME_RESOURCE_REQUEST_POLICY_DELEGATE_H_
#define CHROME_RENDERER_EXTENSIONS_CHROME_RESOURCE_REQUEST_POLICY_DELEGATE_H_

#include "extensions/renderer/resource_request_policy.h"

namespace extensions {

class ChromeResourceRequestPolicyDelegate
    : public ResourceRequestPolicy::Delegate {
 public:
  ChromeResourceRequestPolicyDelegate() = default;
  ChromeResourceRequestPolicyDelegate(
      const ChromeResourceRequestPolicyDelegate&) = delete;
  const ChromeResourceRequestPolicyDelegate& operator=(
      const ChromeResourceRequestPolicyDelegate&) = delete;
  ~ChromeResourceRequestPolicyDelegate() override;

  // ResourceRequestPolicy::Delegate:
  bool ShouldAlwaysAllowRequestForFrameOrigin(
      const url::Origin& frame_origin) override;
  bool AllowLoadForDevToolsPage(const GURL& page_origin,
                                const GURL& target_url) override;
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_CHROME_RESOURCE_REQUEST_POLICY_DELEGATE_H_
