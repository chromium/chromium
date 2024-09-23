// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_deduplication/docs_url_strip_handler.h"

#include <string>
#include <vector>

#include "base/containers/fixed_flat_set.h"
#include "base/containers/lru_cache.h"
#include "base/no_destructor.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "components/url_formatter/url_formatter.h"
#include "net/base/url_util.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/gurl.h"

// TODO(crbug.com/353966074) There is a plan to avoid/consolidate any
//  duplicated code as this borrows from:
//  components/omnibox/browser/document_provider.cc
namespace {
// Verify if the host could possibly be for a valid doc URL. This is a more
// lightweight check than `ExtractDocIdFromUrl()`. It can be done before
// unescaping the URL as valid hosts don't contain escapable chars; unescaping
// is relatively expensive. E.g., 'docs.google.com' isn't a valid doc URL, but
// it's host looks like it could be, so return true. On the other hand,
// 'google.com' is definitely not a doc URL so return false.
bool ValidHostPrefix(const std::string& host) {
  // There are 66 (5*11) valid, e.g. 'docs5.google.com', so rather than check
  // all 66, we just check the 6 prefixes. Keep these prefixes consistent with
  // those in `ExtractDocIdFromUrl()`.
  constexpr auto kValidHostPrefixes = base::MakeFixedFlatSet<std::string_view>({
      "spreadsheets",
      "docs",
      "drive",
      "script",
      "sites",
      "jamboard",
  });
  for (const auto& valid_host_prefix : kValidHostPrefixes) {
    if (base::StartsWith(host, valid_host_prefix,
                         base::CompareCase::INSENSITIVE_ASCII)) {
      return true;
    }
  }
  return false;
}

// Derived from google3/apps/share/util/docs_url_extractor.cc.
std::string ExtractDocIdFromUrl(const std::string& url) {
  static const base::NoDestructor<RE2> docs_url_pattern(
      "\\b("  // The first groups matches the whole URL.
      // Domain.
      "(?:https?://)?(?:"
      // Keep the hosts consistent with `ValidHostPrefix()`.
      "spreadsheets|docs|drive|script|sites|jamboard"
      ")[0-9]?\\.google\\.com"
      "(?::[0-9]+)?\\/"  // Port.
      "(?:\\S*)"         // Non-whitespace chars.
      "(?:"
      // Doc url prefix to match /d/{id}. (?:e/)? deviates from google3.
      "(?:/d/(?:e/)?(?P<path_docid>[0-9a-zA-Z\\-\\_]+))"
      "|"
      // Docs id expr to match a valid id parameter.
      "(?:(?:\\?|&|&amp;)"
      "(?:id|docid|key|docID|DocId)=(?P<query_docid>[0-9a-zA-Z\\-\\_]+))"
      "|"
      // Folder url prefix to match /folders/{folder_id}.
      "(?:/folders/(?P<folder_docid>[0-9a-zA-Z\\-\\_]+))"
      "|"
      // Sites url prefix.
      "(?:/?s/)(?P<sites_docid>[0-9a-zA-Z\\-\\_]+)"
      "(?:/p/[0-9a-zA-Z\\-\\_]+)?/edit"
      "|"
      // Jam url.
      "(?:d/)(?P<jam_docid>[0-9a-zA-Z\\-\\_]+)/(?:edit|viewer)"
      ")"
      // Other valid chars.
      "(?:[0-9a-zA-Z$\\-\\_\\.\\+\\!\\*\'\\,;:@&=/\\?]*)"
      // Summarization details.
      "(?:summarizationDetails=[0-9a-zA-Z$\\-\\_\\.\\+\\!\\*\'\\,;:@&=/"
      "\\?(?:%5B)(?:%5D)]*)?"
      // Other valid chars.
      "(?:[0-9a-zA-Z$\\-\\_\\.\\+\\!\\*\'\\,;:@&=/\\?]*)"
      "(?:(#[0-9a-zA-Z$\\-\\_\\.\\+\\!\\*\'\\,;:@&=/\\?]+)?)"  // Fragment
      ")");

  std::vector<std::string_view> matched_doc_ids(
      docs_url_pattern->NumberOfCapturingGroups() + 1);
  // ANCHOR_START deviates from google3 which uses UNANCHORED. Using
  // ANCHOR_START prevents incorrectly matching with non-drive URLs but which
  // contain a drive URL; e.g.,
  // url-parser.com/?url=https://docs.google.com/document/d/(id)/edit.
  if (!docs_url_pattern->Match(url, 0, url.size(), RE2::ANCHOR_START,
                               matched_doc_ids.data(),
                               matched_doc_ids.size())) {
    return std::string();
  }
  for (const auto& doc_id_group : docs_url_pattern->NamedCapturingGroups()) {
    std::string_view identified_doc_id = matched_doc_ids[doc_id_group.second];
    if (!identified_doc_id.empty()) {
      return std::string(identified_doc_id);
    }
  }
  return std::string();
}
}  // namespace

namespace url_deduplication {

GURL DocsURLStripHandler::StripExtraParams(GURL url) {
  if (!url.is_valid()) {
    return GURL();
  }

  // A memoization cache. Only updated if `ExtractDocIdFromUrl()` was attempted.
  // That's the most expensive part of this algorithm, and memoizing the earlier
  // trivial checks would worsen performance by pushing out more useful cache
  // entries.
  static base::NoDestructor<base::LRUCache<GURL, GURL>> cache(10);
  const auto& cached = cache->Get(url);
  if (cached != cache->end()) {
    return cached->second;
  }

  // Early exit to avoid unnecessary and more involved checks. Don't update the
  // cache for trivial cases to avoid pushing out a more useful entry.
  if (!url.DomainIs("google.com")) {
    return GURL();
  }

  // We aim to prevent duplicate Drive URLs to appear between the Drive document
  // search provider and history/bookmark entries.
  // All URLs are canonicalized to a GURL form only used for deduplication and
  // not guaranteed to be usable for navigation.

  // Drive redirects are already handled by the regex in |ExtractDocIdFromUrl|.
  // The below logic handles google.com redirects; e.g., google.com/url/q=<url>
  std::string url_str;
  std::string url_str_host;
  if (url.host() == "www.google.com" && url.path() == "/url") {
    if ((!net::GetValueForKeyInQuery(url, "q", &url_str) || url_str.empty()) &&
        (!net::GetValueForKeyInQuery(url, "url", &url_str) ||
         url_str.empty())) {
      return GURL();
    }
    url_str_host = GURL(url_str).host();
  } else {
    url_str = url.spec();
    url_str_host = url.host();
  }

  // Recheck the domain, since a google URL could redirect to a non-google URL
  if (!base::EndsWith(url_str_host, "google.com",
                      base::CompareCase::INSENSITIVE_ASCII)) {
    return GURL();
  }

  // Filter out non-doc hosts. Do this before unescaping the URL below, as
  // unescaping can be expensive and valid hosts don't contain escapable chars.
  // Do this after simplifying the google.com redirect above, as that changes
  // the host.
  if (!ValidHostPrefix(url_str_host)) {
    return GURL();
  }

  // Unescape |url_str|
  url_str = base::UnescapeURLComponent(
      url_str,
      base::UnescapeRule::PATH_SEPARATORS |
          base::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS);

  const std::string id = ExtractDocIdFromUrl(url_str);

  // Canonicalize to the /open form without any extra args.
  // This is similar to what we expect from the server.
  GURL deduping_url =
      id.empty() ? GURL() : GURL("https://drive.google.com/open?id=" + id);
  cache->Put(url, deduping_url);
  return deduping_url;
}

}  // namespace url_deduplication
