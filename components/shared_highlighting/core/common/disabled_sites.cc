// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unordered_set>

#include "components/shared_highlighting/core/common/disabled_sites.h"

#include "base/containers/fixed_flat_map.h"
#include "base/feature_list.h"
#include "base/strings/string_util.h"
#include "components/shared_highlighting/core/common/shared_highlighting_features.h"
#include "third_party/re2/src/re2/re2.h"

namespace shared_highlighting {
static auto CreateBlocklist() {
  if (base::FeatureList::IsEnabled(kSharedHighlightingRefinedBlocklist)) {
    return base::MakeFixedFlatMap<base::StringPiece, base::StringPiece>(
        {{"facebook.com", "(?!.*(about)).*"},
         // TODO(crbug.com/1157981): special case this to cover other Google
         // TLDs
         {"google.com", "^\\/amp\\/.*"},
         {"instagram.com", "(?!.*(/p/)).*"},
         {"mail.google.com", ".*"},
         {"outlook.live.com", ".*"},
         {"reddit.com", "(?!.*(comments)).*"},
         {"twitter.com", "(?!.*(status)).*"},
         {"web.whatsapp.com", ".*"},
         {"youtube.com", "?!.*(about|community)).*"}});
  } else {
    return base::MakeFixedFlatMap<base::StringPiece, base::StringPiece>(
        {{"facebook.com", ".*"},
         // TODO(crbug.com/1157981): special case this to cover other Google
         // TLDs
         {"google.com", "^\\/amp\\/.*"},
         {"instagram.com", ".*"},
         {"mail.google.com", ".*"},
         {"outlook.live.com", ".*"},
         {"reddit.com", ".*"},
         {"twitter.com", ".*"},
         {"web.whatsapp.com", ".*"},
         {"youtube.com", ".*"}});
  }
}

bool ShouldOfferLinkToText(const GURL& url) {
  // If a URL's host matches a key in this map, then the path will be tested
  // against the RE stored in the value. For example, {"foo.com", ".*"} means
  // any page on the foo.com domain.

  static auto kBlocklist = CreateBlocklist();

  std::string domain = url.host();
  if (domain.compare(0, 4, "www.") == 0) {
    domain = domain.substr(4);
  } else if (domain.compare(0, 2, "m.") == 0) {
    domain = domain.substr(2);
  } else if (domain.compare(0, 7, "mobile.") == 0) {
    domain = domain.substr(7);
  }

  if (base::FeatureList::IsEnabled(kSharedHighlightingAmp) &&
      domain.compare("google.com") == 0) {
    return true;
  }

  auto* it = kBlocklist.find(domain);
  if (it != kBlocklist.end()) {
    return !re2::RE2::FullMatch(url.path(), it->second.data());
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
