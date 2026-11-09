// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/remote_actor/remote_actor_utils.h"

#include "base/strings/strcat.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace password_manager {

std::optional<url::Origin> GetOriginFromDomainOrOrigin(
    std::string_view domain_or_origin) {
  const GURL url =
      domain_or_origin.contains(url::kStandardSchemeSeparator)
          ? GURL(domain_or_origin)
          : GURL(base::StrCat({url::kHttpsScheme, url::kStandardSchemeSeparator,
                               domain_or_origin}));
  // Remote actors only send HTTP(S) origins.
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS()) {
    return std::nullopt;
  }
  return url::Origin::Create(url);
}

}  // namespace password_manager
