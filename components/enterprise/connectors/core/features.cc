// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/features.h"

#include "build/build_config.h"

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
BASE_FEATURE(kGlicBulkDataEntrySupport, base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
// Controls whether files attached to web pages (file picker, drag-and-drop)
// are scanned by enterprise content analysis on Clank.
BASE_FEATURE(kEnableFileAttachedEnterpriseScanOnClank,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Controls whether cancellation of uploads is enabled for content analysis.
//
// Only enabled by default on desktop, which is the only platform where upload
// cancellation is implemented, tested, and launched via Finch: cancellation is
// driven by `ContentAnalysisDelegate` and `DeepScanningRequest`, which are
// built behind `enterprise_cloud_content_analysis` in //chrome/browser and have
// no mobile equivalent. On Android and iOS, the core scanning code is compiled
// but nothing ever cancels an in-flight upload, so enabling this feature would
// only trigger cancellation reporting from `FilesRequestHandlerBase`'s
// destructor without actually cancelling the scan.
//
// TODO(crbug.com/399639311): Implement and test upload cancellation on Android
// and iOS, then enable this feature on all platforms.
BASE_FEATURE(kEnableCancelUploadOnContentAnalysis,
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

// Controls whether a user cancelling a content analysis scan immediately stops
// in-progress file opening and hashing. See features.h for details.
BASE_FEATURE(kNonBlockingFileOpeningJobCancel,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableAuditOnlyNetworkRequestConnector,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kContentAnalysisClipboardCopy, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableDlpFileSystemApi, base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace enterprise_connectors
