// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/intelligent_scan_delegate.h"

namespace safe_browsing {

// static
IntelligentScanDelegate::IntelligentScanResult
IntelligentScanDelegate::IntelligentScanResult::Failure(
    int model_version,
    ModelType model_type,
    IntelligentScanInfo::NoInfoReason no_info_reason) {
  IntelligentScanResult result;
  result.brand = "";
  result.intent = "";
  result.model_version = model_version;
  result.execution_success = false;
  result.model_type = model_type;
  result.no_info_reason = no_info_reason;
  return result;
}

// static
IntelligentScanDelegate::IntelligentScanResult
IntelligentScanDelegate::IntelligentScanResult::Success(std::string brand,
                                                        std::string intent,
                                                        int model_version,
                                                        ModelType model_type) {
  IntelligentScanResult result;
  result.brand = brand;
  result.intent = intent;
  result.model_version = model_version;
  result.execution_success = true;
  result.model_type = model_type;
  result.no_info_reason = IntelligentScanInfo::NO_INFO_REASON_UNSPECIFIED;
  return result;
}

IntelligentScanDelegate::IntelligentScanResult::IntelligentScanResult() =
    default;
IntelligentScanDelegate::IntelligentScanResult::IntelligentScanResult(
    const IntelligentScanResult& other) = default;
IntelligentScanDelegate::IntelligentScanResult&
IntelligentScanDelegate::IntelligentScanResult::operator=(
    const IntelligentScanResult& other) = default;

// static
bool IntelligentScanDelegate::IsIntelligentScanAvailable(ModelType model_type) {
  switch (model_type) {
    case ModelType::kNotSupportedOnDevice:
    case ModelType::kNotSupportedServerSide:
      return false;
    case ModelType::kOnDevice:
    case ModelType::kServerSide:
      return true;
  }
}

}  // namespace safe_browsing
