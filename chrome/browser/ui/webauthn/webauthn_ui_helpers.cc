// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webauthn/webauthn_ui_helpers.h"

#include "base/strings/strcat.h"
#include "components/url_formatter/elide_url.h"
#include "ui/gfx/font_list.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace webauthn_ui_helpers {

std::u16string RpIdToElidedHost(const std::string& relying_party_id,
                                size_t width) {
  GURL relying_party_id_url(base::StrCat(
      {url::kHttpsScheme, url::kStandardSchemeSeparator, relying_party_id}));
  DCHECK(relying_party_id_url.is_valid());
  return url_formatter::ElideHost(relying_party_id_url, gfx::FontList(), width);
}

}  // namespace webauthn_ui_helpers
