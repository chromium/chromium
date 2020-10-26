// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/shared_highlighting/core/common/text_fragments_utils.h"

#include <sstream>

#include "base/strings/string_util.h"
#include "components/shared_highlighting/core/common/text_fragment.h"
#include "components/shared_highlighting/core/common/text_fragments_constants.h"

namespace shared_highlighting {

GURL AppendFragmentDirectives(const GURL& base_url,
                              std::vector<TextFragment> fragments) {
  if (!base_url.is_valid()) {
    return GURL();
  }

  std::vector<std::string> fragment_strings;
  for (auto it = std::begin(fragments); it != std::end(fragments); ++it) {
    std::string fragment_string = (*it).ToString();
    if (!fragment_string.empty()) {
      fragment_strings.push_back(fragment_string);
    }
  }

  std::string fragments_string = base::JoinString(fragment_strings, "&");
  if (fragments_string.empty()) {
    return base_url;
  }

  std::string new_ref = base_url.ref();
  if (new_ref.find(kFragmentsUrlDelimiter) == std::string::npos) {
    new_ref += kFragmentsUrlDelimiter;
  } else {
    // The URL already had the :~: delimiter, so prepare appending the new
    // fragments by appending an ampersand beforehand.
    new_ref += "&";
  }

  new_ref += fragments_string;

  GURL::Replacements replacements;
  replacements.SetRef(new_ref.c_str(), url::Component(0, new_ref.size()));

  return base_url.ReplaceComponents(replacements);
}

}  // namespace shared_highlighting
