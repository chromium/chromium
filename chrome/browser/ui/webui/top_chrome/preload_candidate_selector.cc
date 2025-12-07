// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/top_chrome/preload_candidate_selector.h"

#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/no_destructor.h"
#include "base/strings/string_split.h"
#include "chrome/browser/ui/ui_features.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

std::vector<url::Origin> GetOriginsExcludedByFlag() {
  std::vector<std::string> origin_strings =
      base::SplitString(features::kPreloadTopChromeWebUIExcludeOrigins.Get(),
                        ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  std::vector<url::Origin> origins;
  for (const std::string& origin_string : origin_strings) {
    origins.push_back(url::Origin::Create(GURL(origin_string)));
  }
  return origins;
}

}  // namespace

namespace webui {

bool PreloadCandidateSelector::IsUrlExcludedByFlag(const GURL& url) const {
  static base::NoDestructor<std::vector<url::Origin>> excluded_origins_(
      GetOriginsExcludedByFlag());
  return base::Contains(*excluded_origins_, url::Origin::Create(url));
}

}  // namespace webui
