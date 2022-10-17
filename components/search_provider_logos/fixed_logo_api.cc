// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_provider_logos/fixed_logo_api.h"

#include <memory>

#include "base/memory/ref_counted_memory.h"
#include "components/search_provider_logos/logo_common.h"
#include "url/gurl.h"

namespace search_provider_logos {

std::unique_ptr<EncodedLogo> ParseFixedLogoResponse(
    std::unique_ptr<std::string> response,
    base::Time response_time,
    bool* parsing_failed) {
  auto logo = std::make_unique<EncodedLogo>();
  logo->encoded_image =
      base::MakeRefCounted<base::RefCountedString>(std::move(*response));

  // If |can_show_after_expiration| is true, the |expiration_time| has little
  // effect. Set it as far as possible in the future just as an approximation.
  logo->metadata.expiration_time =
      response_time + base::Milliseconds(kMaxTimeToLiveMS);
  logo->metadata.can_show_after_expiration = true;

  *parsing_failed = false;
  return logo;
}

GURL UseFixedLogoUrl(const GURL& logo_url, const std::string& fingerprint) {
  return logo_url;
}

}  // namespace search_provider_logos
