// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/shared_highlighting/core/common/disabled_sites.h"

#include <string_view>
#include <unordered_set>

#include "base/containers/fixed_flat_map.h"
#include "base/feature_list.h"
#include "base/strings/string_util.h"
#include "components/shared_highlighting/core/common/shared_highlighting_features.h"
#include "third_party/re2/src/re2/re2.h"

namespace shared_highlighting {

namespace {

bool IsAmpGenerationEnabled() {
#if BUILDFLAG(IS_IOS)
  return base::FeatureList::IsEnabled(kSharedHighlightingAmp);
#else
  return true;
#endif
}

}  // namespace

bool ShouldOfferLinkToText(const GURL& url) {
  // If a URL's host matches a key in the map, then the path will be tested
  // against the RE stored in the value. For example, {"foo.com", ".*"} means
  // any page on the foo.com domain.
  static constexpr auto kBlocklist =
      base::MakeFixedFlatMap<std::string_view, std::string_view>(
          {{"facebook.com", ".*"},
           // TODO(crbug.com/40736718): special case this to cover other Google
           // TLDs
           {"google.com", "^\\/amp\\/.*"},
           {"instagram.com", ".*"},
           {"mail.google.com", ".*"},
           {"outlook.live.com", ".*"},
           {"reddit.com", ".*"},
           {"twitter.com", ".*"},
           {"web.whatsapp.com", ".*"},
           {"youtube.com", ".*"}});

  std::string domain = url.host();
  if (domain.compare(0, 4, "www.") == 0) {
    domain = domain.substr(4);
  } else if (domain.compare(0, 2, "m.") == 0) {
    domain = domain.substr(2);
  } else if (domain.compare(0, 7, "mobile.") == 0) {
    domain = domain.substr(7);
  }

  if (IsAmpGenerationEnabled() && domain.compare("google.com") == 0) {
    return true;
  }

  auto block_list_it = kBlocklist.find(domain);
  if (block_list_it != kBlocklist.end()) {
    if (re2::RE2::FullMatch(url.path(), block_list_it->second.data())) {
      return false;
    }
  }
  return true;
}

bool SupportsLinkGenerationInIframe(GURL main_frame_url) {
  const std::unordered_set<std::string> good_hosts = {
      "www.google.com", "m.google.com", "mobile.google.com",
      "www.bing.com",   "m.bing.com",   "mobile.bing.com"};

  return main_frame_url.SchemeIs(url::kHttpsScheme) &&
         good_hosts.find(main_frame_url.host()) != good_hosts.end() &&
         base::StartsWith(main_frame_url.path(), "/amp/");
}

}  // namespace shared_highlighting
