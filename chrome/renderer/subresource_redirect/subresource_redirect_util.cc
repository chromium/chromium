// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/subresource_redirect/subresource_redirect_util.h"

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/renderer/subresource_redirect/subresource_redirect_params.h"
#include "components/base32/base32.h"
#include "crypto/sha2.h"
#include "net/base/escape.h"
#include "url/gurl.h"

namespace subresource_redirect {

GURL GetSubresourceURLForURL(const GURL& original_url) {
  DCHECK(original_url.is_valid());

  GURL compressed_url = GetSubresourceRedirectOrigin().GetURL();
  std::string origin_hash = base::ToLowerASCII(base32::Base32Encode(
      crypto::SHA256HashString(
          original_url.scheme() + "://" + original_url.host() + ":" +
          base::NumberToString(original_url.EffectiveIntPort())),
      base32::Base32EncodePolicy::OMIT_PADDING));
  std::string host_str = origin_hash + "." + compressed_url.host();
  std::string query_str =
      "u=" + net::EscapeQueryParamValue(original_url.GetAsReferrer().spec(),
                                        true /* use_plus */);
  std::string ref_str = original_url.ref();

  GURL::Replacements replacements;
  replacements.SetHostStr(host_str);
  replacements.SetPathStr("/i");
  replacements.SetQueryStr(query_str);
  if (!ref_str.empty())
    replacements.SetRefStr(ref_str);

  compressed_url = compressed_url.ReplaceComponents(replacements);
  DCHECK(compressed_url.is_valid());
  return compressed_url;
}

}  // namespace subresource_redirect
