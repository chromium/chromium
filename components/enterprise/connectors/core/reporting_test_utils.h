// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_REPORTING_TEST_UTILS_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_REPORTING_TEST_UTILS_H_

#include <map>
#include <set>
#include <string>
#include <vector>

class PrefService;

namespace enterprise_connectors::test {

// Helper function to set "OnSecurityEventEnterpriseConnector" for tests.
void SetOnSecurityEventReporting(
    PrefService* prefs,
    bool enabled,
    const std::set<std::string>& enabled_event_names = std::set<std::string>(),
    const std::map<std::string, std::vector<std::string>>&
        enabled_opt_in_events =
            std::map<std::string, std::vector<std::string>>(),
    bool machine_scope = true);

}  // namespace enterprise_connectors::test

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_REPORTING_TEST_UTILS_H_
