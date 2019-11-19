// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/subresource_redirect/subresource_redirect_experiments.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_split.h"
#include "third_party/blink/public/common/features.h"

namespace subresource_redirect {

bool ShouldIncludeMediaSuffix(const GURL& url) {
  std::vector<std::string> suffixes = {".jpg", ".jpeg", ".png", ".svg",
                                       ".webp"};

  std::string csv = base::GetFieldTrialParamValueByFeature(
      blink::features::kSubresourceRedirect, "included_path_suffixes");
  if (csv != "") {
    suffixes = base::SplitString(csv, ",", base::TRIM_WHITESPACE,
                                 base::SPLIT_WANT_NONEMPTY);
  }

  for (const std::string& suffix : suffixes) {
    if (base::EndsWith(url.path(), suffix,
                       base::CompareCase::INSENSITIVE_ASCII)) {
      return true;
    }
  }
  return false;
}

}  // namespace subresource_redirect
