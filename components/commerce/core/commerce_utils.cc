// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/commerce_utils.h"

#include "net/base/url_util.h"
#include "url/gurl.h"

namespace commerce {

namespace {
// TODO(b:289242951): Update the utm tag and value once they are finalized.
constexpr char kUtmSourceTag[] = "utm_source";
constexpr char kUtmSourceValue[] = "chrome-history-cluster-with-discount";
}  // namespace

bool UrlContainsDiscountUtmTag(const GURL& url) {
  std::string utm_name;
  if (!net::GetValueForKeyInQuery(url, kUtmSourceTag, &utm_name)) {
    return false;
  }
  return utm_name == kUtmSourceValue;
}

}  // namespace commerce
