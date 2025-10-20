// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_FEATURES_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace enterprise_connectors {

// Controls whether enterprise features will attempt to attach the active
// content area user email to DLP/reporting requests on Workspace sites.
BASE_DECLARE_FEATURE(kEnterpriseActiveUserDetection);

// Controls whether the iFrame parent url chain initiated from the active frame
// will be attached to DLP scan requests.
BASE_DECLARE_FEATURE(kEnterpriseIframeDlpRulesSupport);

// Controls whether resumable upload is enabled on consumer scans.
BASE_DECLARE_FEATURE(kEnableResumableUploadOnConsumerScan);

// Controls the new upload, download, and print size limit for content analysis.
BASE_DECLARE_FEATURE(kEnableNewUploadSizeLimit);

// Controls the maximum file size for content analysis in MB.
BASE_DECLARE_FEATURE_PARAM(size_t, kMaxContentAnalysisFileSizeMB);

// Controls whether encrypted file upload is enabled.
BASE_DECLARE_FEATURE(kEnableEncryptedFileUpload);

BASE_DECLARE_FEATURE(kDlpScanPastedImages);

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_FEATURES_H_
