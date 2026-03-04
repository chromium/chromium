// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_DEEP_SCANNING_UTILS_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_DEEP_SCANNING_UTILS_H_

#include "components/enterprise/connectors/core/cloud_content_scanning/common.h"
#include "components/enterprise/connectors/core/content_analysis_info_base.h"
#include "components/enterprise/connectors/core/reporting_event_router.h"

namespace enterprise_connectors {

// Helper function to examine a ContentAnalysisResponse and report the
// appropriate events to the enterprise admin. |download_digest_sha256| must be
// encoded using base::HexEncode.  |event_result| indicates whether the user was
// ultimately allowed to access the text or file.
void MaybeReportDeepScanningVerdict(
    ReportingEventRouter* reporting_event_router,
    const ContentAnalysisInfoBase* content_analysis_info,
    const std::string& source,
    const std::string& destination,
    const std::string& file_name,
    const std::string& download_digest_sha256,
    const std::string& mime_type,
    const std::string& trigger,
    const std::string& content_transfer_method,
    const std::string& source_email,
    const int64_t content_size,
    ScanRequestUploadResult result,
    const ContentAnalysisResponse& response,
    EventResult event_result);

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_DEEP_SCANNING_UTILS_H_
