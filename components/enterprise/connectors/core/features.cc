// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/features.h"

namespace enterprise_connectors {

BASE_FEATURE(kEnterpriseIframeDlpRulesSupport,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableResumableUploadOnConsumerScan,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kContentHashInFileUploadFinalCall,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls the new upload, download and print size limit for content analysis.
BASE_FEATURE(kEnableNewUploadSizeLimit, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(size_t,
                   kMaxContentAnalysisFileSizeMB,
                   &kEnableNewUploadSizeLimit,
                   "max_file_size_mb",
                   /*default_value=*/50);

// Controls the new upload count limit for content analysis.
BASE_FEATURE(kEnableNewUploadCountLimit, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(size_t,
                   kParallelContentAnalysisRequestCountMax,
                   &kEnableNewUploadCountLimit,
                   "max_parallel_requests",
                   /*default_value=*/kDefaultMaxParallelActiveRequests);

// Controls whether encrypted file upload is enabled.
BASE_FEATURE(kEnableEncryptedFileUpload, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables scanning of pasted images for DLP.
BASE_FEATURE(kDlpScanPastedImages, base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace enterprise_connectors
