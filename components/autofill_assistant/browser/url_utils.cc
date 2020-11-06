// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/url_utils.h"

#include <algorithm>

#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"

namespace {

bool IsInSubDomain(const GURL& url, const std::string& domain) {
  return base::EndsWith(base::StringPiece(url.host()),
                        base::StringPiece("." + domain),
                        base::CompareCase::INSENSITIVE_ASCII);
}
}  // namespace

namespace autofill_assistant {
namespace url_utils {

bool IsInDomainOrSubDomain(const GURL& url, const GURL& domain) {
  if (url.host() == domain.host()) {
    return true;
  }

  return IsInSubDomain(url, domain.host());
}

bool IsInDomainOrSubDomain(const GURL& url,
                           const std::vector<std::string>& allowed_domains) {
  return std::find_if(allowed_domains.begin(), allowed_domains.end(),
                      [url](const std::string& allowed_domain) {
                        return url.host() == allowed_domain ||
                               IsInSubDomain(url, allowed_domain);
                      }) != allowed_domains.end();
}

}  // namespace url_utils
}  // namespace autofill_assistant
