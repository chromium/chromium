// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/intelligent_scan_delegate.h"

namespace safe_browsing {

// static
IntelligentScanDelegate::IntelligentScanResult
IntelligentScanDelegate::IntelligentScanResult::Failure(int model_version,
                                                        ModelType model_type) {
  return {.brand = "",
          .intent = "",
          .model_version = model_version,
          .execution_success = false,
          .model_type = model_type};
}

}  // namespace safe_browsing
