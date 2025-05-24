// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/url_utils.h"

#include <string>
#include <string_view>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/uuid.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/grit/components_resources.h"
#include "crypto/sha2.h"
#include "net/base/url_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"
#include "url/url_util.h"

namespace dom_distiller {

namespace url_utils {

namespace {

const char kDummyInternalUrlPrefix[] = "chrome-distiller-internal://dummy/";
const char kSeparator[] = "_";

std::string SHA256InHex(std::string_view str) {
  std::string sha256 = crypto::SHA256HashString(str);
  return base::ToLowerASCII(base::HexEncode(sha256));
}

}  // namespace

const GURL GetDistillerViewUrlFromEntryId(const std::string& scheme,
                                          const std::string& entry_id) {
  GURL url(scheme + "://" + base::Uuid::GenerateRandomV4().AsLowercaseString());
  return net::AppendOrReplaceQueryParameter(url, kEntryIdKey, entry_id);
}

const GURL GetDistillerViewUrlFromUrl(const std::string& scheme,
                                      const GURL& url,
                                      const std::string& title,
                                      int64_t start_time_ms) {
  GURL view_url(scheme + "://" +
                base::Uuid::GenerateRandomV4().AsLowercaseString() +
                kSeparator + SHA256InHex(url.spec()));
  view_url = net::AppendOrReplaceQueryParameter(view_url, kTitleKey, title);
  if (start_time_ms > 0) {
    view_url = net::AppendOrReplaceQueryParameter(
        view_url, kTimeKey, base::NumberToString(start_time_ms));
  }
  return net::AppendOrReplaceQueryParameter(view_url, kUrlKey, url.spec());
}

GURL GetOriginalUrlFromDistillerUrl(const GURL& url) {
  if (!IsUrlDistilledFormat(url))
    return url;

  std::string original_url_str;
  net::GetValueForKeyInQuery(url, kUrlKey, &original_url_str);

  // Make sure kDomDistillerScheme is considered standard scheme for
  // |GURL::host_piece()| to work correctly.
  DCHECK(url::IsStandard(kDomDistillerScheme,
                         url::Component(0, strlen(kDomDistillerScheme))));
  std::vector<std::string_view> pieces =
      base::SplitStringPiece(url.host_piece(), kSeparator,
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (pieces.size() != 2)
    return GURL();
  if (SHA256InHex(original_url_str) != pieces[1])
    return GURL();
  const GURL original_url(original_url_str);
  if (!IsUrlDistillable(original_url))
    return GURL();

  return original_url;
}

int64_t GetTimeFromDistillerUrl(const GURL& url) {
  if (!IsDistilledPage(url))
    return 0;

  std::string time_str;
  if (!net::GetValueForKeyInQuery(url, kTimeKey, &time_str))
    return 0;

  int64_t time_int = 0;
  if (!base::StringToInt64(time_str, &time_int))
    return 0;

  return time_int;
}

std::string GetTitleFromDistillerUrl(const GURL& url) {
  if (!IsDistilledPage(url))
    return "";

  std::string title;
  if (!net::GetValueForKeyInQuery(url, kTitleKey, &title))
    return "";

  return title;
}

std::string GetValueForKeyInUrl(const GURL& url, const std::string& key) {
  if (!url.is_valid())
    return "";
  std::string value;
  if (net::GetValueForKeyInQuery(url, key, &value)) {
    return value;
  }
  return "";
}

std::string GetValueForKeyInUrlPathQuery(const std::string& path,
                                         const std::string& key) {
  // Tools for retrieving a value in a query only works with full GURLs, so
  // using a dummy scheme and host to create a fake URL which can be parsed.
  GURL dummy_url(kDummyInternalUrlPrefix + path);
  return GetValueForKeyInUrl(dummy_url, key);
}

bool IsUrlDistillable(const GURL& url) {
  return url.is_valid() && url.SchemeIsHTTPOrHTTPS();
}

bool IsDistilledPage(const GURL& url) {
  return IsUrlDistilledFormat(url) &&
         GetOriginalUrlFromDistillerUrl(url).is_valid();
}

bool IsUrlDistilledFormat(const GURL& url) {
  return url.is_valid() && url.scheme() == kDomDistillerScheme;
}

}  // namespace url_utils

}  // namespace dom_distiller
