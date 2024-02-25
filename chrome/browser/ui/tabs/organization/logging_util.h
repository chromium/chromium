// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_ORGANIZATION_LOGGING_UTIL_H_
#define CHROME_BROWSER_UI_TABS_ORGANIZATION_LOGGING_UTIL_H_

#include "chrome/browser/ui/tabs/organization/tab_organization_request.h"

namespace optimization_guide {
namespace proto {
class TabOrganizationQuality;
}  // namespace proto
}  // namespace optimization_guide

class TabOrganizationSession;
class TabOrganization;

void AddOrganizationDetailsToQualityOrganization(
    optimization_guide::proto::TabOrganizationQuality* quality,
    const TabOrganization* organization,
    const TabOrganizationResponse::Organization* response_organization);

void AddSessionDetailsToQuality(
    optimization_guide::proto::TabOrganizationQuality* quality,
    const TabOrganizationSession* session);

#endif  // CHROME_BROWSER_UI_TABS_ORGANIZATION_LOGGING_UTIL_H_
