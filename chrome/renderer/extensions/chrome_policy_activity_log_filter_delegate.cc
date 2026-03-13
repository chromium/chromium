// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/chrome_policy_activity_log_filter_delegate.h"

#include "chrome/common/extensions/activity_log_policy_util.h"
#include "extensions/common/dom_action_types.h"
#include "url/gurl.h"

namespace extensions {

ChromePolicyActivityLogFilterDelegate::ChromePolicyActivityLogFilterDelegate() =
    default;
ChromePolicyActivityLogFilterDelegate::
    ~ChromePolicyActivityLogFilterDelegate() = default;

bool ChromePolicyActivityLogFilterDelegate::IsHighRiskEvent(
    const ExtensionId& extension_id,
    DomActionType::Type action_type,
    const std::string& api_name,
    const base::ListValue& args,
    const GURL& url) {
  activity_log_policy_util::TelemetrySignalType signal_type =
      activity_log_policy_util::GetTelemetrySignalType(api_name, args,
                                                       action_type);
  return activity_log_policy_util::IsHighRiskEvent(signal_type);
}

}  // namespace extensions
