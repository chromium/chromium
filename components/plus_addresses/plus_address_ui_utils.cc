// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_ui_utils.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/plus_addresses/plus_address_types.h"
#include "components/url_formatter/elide_url.h"
#include "url/gurl.h"

namespace plus_addresses {

std::u16string GetOriginForDisplay(const PlusProfile& plus_address) {
  if (!plus_address.facet.is_valid()) {
    return std::u16string();
  }

  if (plus_address.facet.IsValidAndroidFacetURI()) {
    return base::UTF8ToUTF16(plus_address.facet.GetAndroidPackageDisplayName());
  }

  // TODO: crbug.com/327838324 - Revisit the `OMIT_CRYPTOGRAPHIC` parameter once
  // the plus address creation support for http domains is launched.
  return url_formatter::FormatUrlForSecurityDisplay(
      GURL(plus_address.facet.canonical_spec()),
      url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);
}

}  // namespace plus_addresses
