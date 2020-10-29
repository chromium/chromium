// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/shared_highlighting/core/common/text_fragments_utils.h"

#include <sstream>

#include "base/json/json_writer.h"
#include "base/optional.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "components/shared_highlighting/core/common/text_fragment.h"
#include "components/shared_highlighting/core/common/text_fragments_constants.h"

namespace shared_highlighting {

base::Value ParseTextFragments(const GURL& url) {
  if (!url.has_ref())
    return {};
  std::vector<std::string> fragments = ExtractTextFragments(url.ref());
  if (fragments.empty())
    return {};

  base::Value parsed(base::Value::Type::LIST);
  for (const std::string& fragment : fragments) {
    base::Optional<TextFragment> opt_frag =
        TextFragment::FromEscapedString(fragment);
    if (opt_frag.has_value()) {
      parsed.Append(opt_frag->ToValue());
    }
  }

  return parsed;
}

std::vector<std::string> ExtractTextFragments(std::string ref_string) {
  size_t start_pos = ref_string.find(kFragmentsUrlDelimiter);
  if (start_pos == std::string::npos)
    return {};
  ref_string.erase(0, start_pos + strlen(kFragmentsUrlDelimiter));

  std::vector<std::string> fragment_strings;
  while (ref_string.size()) {
    // Consume everything up to and including the text= prefix
    size_t prefix_pos = ref_string.find(kFragmentParameterName);
    if (prefix_pos == std::string::npos)
      break;
    ref_string.erase(0, prefix_pos + strlen(kFragmentParameterName));

    // A & indicates the end of the fragment (and the start of the next).
    // Save everything up to this point, and then consume it (including the &).
    size_t ampersand_pos = ref_string.find("&");
    if (ampersand_pos != 0)
      fragment_strings.push_back(ref_string.substr(0, ampersand_pos));
    if (ampersand_pos == std::string::npos)
      break;
    ref_string.erase(0, ampersand_pos + 1);
  }
  return fragment_strings;
}

GURL AppendFragmentDirectives(const GURL& base_url,
                              std::vector<TextFragment> fragments) {
  if (!base_url.is_valid()) {
    return GURL();
  }

  std::vector<std::string> fragment_strings;
  for (auto it = std::begin(fragments); it != std::end(fragments); ++it) {
    std::string fragment_string = (*it).ToEscapedString();
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
    // The URL already had the :~: delimiter, so remove what comes after before
    // adding the new fragment(s).
    new_ref = new_ref.substr(0, new_ref.find(kFragmentsUrlDelimiter) +
                                    strlen(kFragmentsUrlDelimiter));
  }

  new_ref += fragments_string;

  GURL::Replacements replacements;
  replacements.SetRef(new_ref.c_str(), url::Component(0, new_ref.size()));

  return base_url.ReplaceComponents(replacements);
}

}  // namespace shared_highlighting
