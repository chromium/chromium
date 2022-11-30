// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/ui_support.h"

namespace vr {

UScriptCode UScriptGetScript(UChar32 codepoint, UErrorCode* err) {
  return uscript_getScript(codepoint, err);
}

std::u16string FormatUrlForVr(const GURL& gurl, url::Parsed* new_parsed) {
  return url_formatter::FormatUrl(
      gurl,

      url_formatter::kFormatUrlOmitDefaults |
          url_formatter::kFormatUrlOmitHTTPS |
          url_formatter::kFormatUrlOmitTrivialSubdomains,
      base::UnescapeRule::NORMAL, new_parsed, nullptr, nullptr);
}

}  // namespace vr
