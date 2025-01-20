// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_REPORTING_UTILS_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_REPORTING_UTILS_H_

#include "components/enterprise/common/proto/synced/browser_events.pb.h"
#include "components/enterprise/connectors/core/common.h"
#include "url/gurl.h"

namespace enterprise_connectors {

// Helper functions that compiles information into event protos. The
// logic is shared across platforms to ensure event consistency.
//
// PasswordBreachEvent could be empty if none of the `identities` matched a
// pattern in the URL filters.
std::optional<chrome::cros::reporting::proto::PasswordBreachEvent>
GetPasswordBreachEvent(
    const std::string& trigger,
    const std::vector<std::pair<GURL, std::u16string>>& identities,
    const enterprise_connectors::ReportingSettings& settings);

chrome::cros::reporting::proto::SafeBrowsingPasswordReuseEvent
GetPasswordReuseEvent(const GURL& url,
                      const std::string& user_name,
                      bool is_phishing_url,
                      bool warning_shown);

chrome::cros::reporting::proto::SafeBrowsingPasswordChangedEvent
GetPasswordChangedEvent(const std::string& user_name);

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_REPORTING_UTILS_H_
