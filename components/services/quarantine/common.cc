// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/quarantine/common.h"

#include "url/origin.h"
#include "url/url_canon.h"

namespace quarantine {

GURL SanitizeUrlForQuarantine(const GURL& source_url) {
  // Invalid URLs and 'data' URLs don't confer an authority.
  if (!source_url.is_valid() || source_url.SchemeIs("data")) {
    return GURL();
  }

  // The full content of these URLs are only meaningful within the confines of
  // the browser. Origin extracts the inner URL for both of these schemes.
  if (source_url.SchemeIsBlob() || source_url.SchemeIsFileSystem()) {
    return url::Origin::Create(source_url).GetURL();
  }

  if (!source_url.SchemeIsHTTPOrHTTPS() && !source_url.SchemeIsWSOrWSS()) {
    return source_url;
  }

  url::Replacements<char> replacements;
  replacements.ClearUsername();
  replacements.ClearPassword();

  return source_url.ReplaceComponents(replacements);
}

}  // namespace quarantine
