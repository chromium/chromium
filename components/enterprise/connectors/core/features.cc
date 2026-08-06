// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/features.h"

namespace enterprise_connectors {

BASE_FEATURE(kEnterpriseIframeDlpRulesSupport,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableResumableUploadOnConsumerScan,
             base::FEATURE_DISABLED_BY_DEFAULT);

// TODO: crbug.com/535280570 - only clean up after async file hash is validated
// for smaller min threshold.
BASE_FEATURE(kContentHashInFileUploadFinalCall,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls the new upload, download and print size limit for content analysis.
BASE_FEATURE(kEnableNewUploadSizeLimit, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(size_t,
                   kMaxContentAnalysisFileSizeMB,
                   &kEnableNewUploadSizeLimit,
                   "max_file_size_mb",
                   /*default_value=*/250);

// Controls whether encrypted file upload is enabled.
BASE_FEATURE(kEnableEncryptedFileUpload, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables scanning of pasted images for DLP.
BASE_FEATURE(kDlpScanPastedImages, base::FEATURE_ENABLED_BY_DEFAULT);

// Controls enabling bulk data entry support in Glic actuation logic.
BASE_FEATURE(kGlicBulkDataEntrySupport, base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
// Controls whether WebProtect download on Clank is enabled.
BASE_FEATURE(kEnableDownloadEnterpriseScanOnClank,
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// Controls whether cancellation of uploads is enabled for content analysis.
BASE_FEATURE(kEnableCancelUploadOnContentAnalysis,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableAuditOnlyNetworkRequestConnector,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kContentAnalysisClipboardCopy, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableDlpFileSystemApi, base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace enterprise_connectors
