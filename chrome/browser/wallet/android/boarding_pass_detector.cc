// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/wallet/android/boarding_pass_detector.h"

#include "base/feature_list.h"
#include "base/strings/string_split.h"
#include "chrome/common/chrome_features.h"

namespace wallet {

bool BoardingPassDetector::ShouldDetect(const std::string& url) {
  std::string param_val = base::GetFieldTrialParamValueByFeature(
      features::kBoardingPassDetector,
      features::kBoardingPassDetectorUrlParam.name);

  std::vector<std::string> allowed_urls =
      base::SplitString(std::move(param_val), ",", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);
  for (const auto& allowed_url : allowed_urls) {
    if (url.starts_with(allowed_url)) {
      return true;
    }
  }
  return false;
}

}  // namespace wallet
