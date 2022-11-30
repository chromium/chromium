// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/database_utils/url_converter.h"

#include "url/gurl.h"

namespace database_utils {

std::string GurlToDatabaseUrl(const GURL& gurl) {
  // Strip username and password from URL before sending to DB.
  GURL::Replacements replacements;
  replacements.ClearUsername();
  replacements.ClearPassword();
  return (gurl.ReplaceComponents(replacements)).spec();
}

}  // namespace database_utils
