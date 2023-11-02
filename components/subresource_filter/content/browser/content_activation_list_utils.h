// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_CONTENT_ACTIVATION_LIST_UTILS_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_CONTENT_ACTIVATION_LIST_UTILS_H_

#include "components/safe_browsing/core/browser/db/util.h"
#include "components/subresource_filter/core/common/activation_list.h"

namespace subresource_filter {

ActivationList GetListForThreatTypeAndMetadata(
    safe_browsing::SBThreatType threat_type,
    const safe_browsing::ThreatMetadata& threat_type_metadata,
    bool* warning);

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_CONTENT_ACTIVATION_LIST_UTILS_H_
