// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_CHROME_POLICY_ACTIVITY_LOG_FILTER_DELEGATE_H_
#define CHROME_RENDERER_EXTENSIONS_CHROME_POLICY_ACTIVITY_LOG_FILTER_DELEGATE_H_

#include "extensions/renderer/policy_activity_log_filter.h"

namespace extensions {

// Chrome's implementation of the PolicyActivityLogFilter.
//
// This delegate identifies high-risk extension behaviors such as:
// 1. Session Hijacking (e.g., document.cookie access)
// 2. Form Grabbing (e.g., reading input/textarea values)
// 3. Remote Script Injection (e.g., creating <script> tags with external URLs)
// 4. Form Hijacking (e.g., redirecting form actions)
// 5. Malicious Protocol Injection (e.g., javascript: or data: URLs)
class ChromePolicyActivityLogFilterDelegate : public PolicyActivityLogFilter {
 public:
  ChromePolicyActivityLogFilterDelegate();
  ~ChromePolicyActivityLogFilterDelegate() override;

  // PolicyActivityLogFilter implementation.
  bool IsHighRiskEvent(const ExtensionId& extension_id,
                       DomActionType::Type action_type,
                       const std::string& api_name,
                       const base::ListValue& args,
                       const GURL& url) override;
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_CHROME_POLICY_ACTIVITY_LOG_FILTER_DELEGATE_H_
