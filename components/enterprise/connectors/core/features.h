// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_FEATURES_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace enterprise_connectors {

// Controls whether the iFrame parent url chain initiated from the active frame
// will be attached to DLP scan requests.
BASE_DECLARE_FEATURE(kEnterpriseIframeDlpRulesSupport);

// Controls whether resumable upload is enabled on consumer scans.
BASE_DECLARE_FEATURE(kEnableResumableUploadOnConsumerScan);

// Controls whether hash of resumable uploads is uploaded in the final call for
// large files.
BASE_DECLARE_FEATURE(kContentHashInFileUploadFinalCall);

// Controls the new upload, download, and print size limit for content analysis.
BASE_DECLARE_FEATURE(kEnableNewUploadSizeLimit);

// Controls the maximum file size for content analysis in MB.
BASE_DECLARE_FEATURE_PARAM(size_t, kMaxContentAnalysisFileSizeMB);

// The default maximum number of concurrent active requests. This is used to
// limit the number of requests that are actively being uploaded. This is set to
// default of 30 because it was determined to be a good value through
// experiments. See http://crbug.com/444472089.
inline constexpr int kDefaultMaxParallelActiveRequests = 30;

// Controls enabling and count of concurrent upload limit for content analysis.
BASE_DECLARE_FEATURE(kEnableNewUploadCountLimit);
BASE_DECLARE_FEATURE_PARAM(size_t, kParallelContentAnalysisRequestCountMax);

// Controls whether encrypted file upload is enabled.
BASE_DECLARE_FEATURE(kEnableEncryptedFileUpload);

BASE_DECLARE_FEATURE(kDlpScanPastedImages);

// Controls enabling bulk data entry support in Glic actuation logic.
BASE_DECLARE_FEATURE(kGlicBulkDataEntrySupport);

#if BUILDFLAG(IS_ANDROID)
// Controls whether WebProtect download on Clank is enabled.
BASE_DECLARE_FEATURE(kEnableDownloadEnterpriseScanOnClank);
#endif

// Controls whether cancellation of uploads is enabled for content analysis.
BASE_DECLARE_FEATURE(kEnableCancelUploadOnContentAnalysis);

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_FEATURES_H_
