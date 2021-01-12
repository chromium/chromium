// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/shared_highlighting/core/common/disabled_sites.h"

#include "third_party/re2/src/re2/re2.h"

#include <map>
#include <utility>

namespace shared_highlighting {

bool ShouldOfferLinkToText(const GURL& url) {
  // If a URL's host matches a key in this map, then the path will be tested
  // against the RE stored in the value. For example, {"foo.com", ".*"} means
  // any page on the foo.com domain.
  const static std::map<std::string, std::string> kBlocklist = {
      {"youtube.com", ".*"},
      // TODO(crbug.com/1157981): special case this to cover other Google TLDs
      {"google.com", "^\\/amp\\/.*"}};

  const std::string domain =
      url.host().compare(0, 4, "www.") == 0 ? url.host().substr(4) : url.host();
  auto it = kBlocklist.find(domain);
  if (it != kBlocklist.end()) {
    return !re2::RE2::FullMatch(url.path(), it->second);
  }
  return true;
}

}  // namespace shared_highlighting
