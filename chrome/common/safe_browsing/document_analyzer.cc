// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/safe_browsing/document_analyzer.h"

#include <memory>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/i18n/streaming_utf8_validator.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/common/safe_browsing/document_analyzer_results.h"
#include "chrome/common/safe_browsing/download_type_util.h"
#include "components/safe_browsing/content/common/file_type_policies.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"

namespace safe_browsing {
namespace document_analyzer {

void AnalyzeDocument(const std::string& office_file,
                     const base::FilePath& file_path,
                     DocumentAnalyzerResults* results) {
  NOTIMPLEMENTED();
  results->has_macros = false;
  results->success = false;
  results->error_code = ClientDownloadRequest::DocumentProcessingInfo::UNKNOWN;
  results->error_message = std::string();
}
}  // namespace document_analyzer
}  // namespace safe_browsing
