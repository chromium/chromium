// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_rewrite/browser/url_request_rewrite_rules_validation.h"

#include <string_view>

#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "net/http/http_util.h"
#include "url/url_constants.h"

namespace url_rewrite {
namespace {

bool IsValidUrlHost(std::string_view host) {
  return GURL(base::StrCat({url::kHttpScheme, "://", host})).is_valid();
}

bool ValidateAddHeaders(
    const mojom::UrlRequestRewriteAddHeadersPtr& add_headers) {
  if (!add_headers || add_headers->headers.empty()) {
    LOG(ERROR) << "Add headers is missing";
    return false;
  }
  return base::ranges::all_of(
      add_headers->headers, [](const mojom::UrlHeaderPtr& header) {
        if (!net::HttpUtil::IsValidHeaderName(header->name)) {
          LOG(ERROR) << "Invalid header name: " << header->name;
          return false;
        }
        if (!net::HttpUtil::IsValidHeaderValue(header->value)) {
          LOG(ERROR) << "Invalid header value: " << header->value;
          return false;
        }
        return true;
      });
}

bool ValidateRemoveHeader(
    const mojom::UrlRequestRewriteRemoveHeaderPtr& remove_header) {
  if (!remove_header) {
    LOG(ERROR) << "Remove headers is missing";
    return false;
  }
  if (!net::HttpUtil::IsValidHeaderName(remove_header->header_name)) {
    LOG(ERROR) << "Invalid header name: " << remove_header->header_name;
    return false;
  }
  return true;
}

bool ValidateSubstituteQueryPattern(
    const mojom::UrlRequestRewriteSubstituteQueryPatternPtr&
        substitute_query_pattern) {
  if (!substitute_query_pattern) {
    LOG(ERROR) << "Substitute query pattern is missing";
    return false;
  }
  if (substitute_query_pattern->pattern.empty()) {
    LOG(ERROR) << "Substitute query pattern is empty";
    return false;
  }
  return true;
}

bool ValidateReplaceUrl(
    const mojom::UrlRequestRewriteReplaceUrlPtr& replace_url) {
  if (!replace_url) {
    LOG(ERROR) << "Replace url is missing";
    return false;
  }
  if (!GURL("http://site.com/" + replace_url->url_ends_with).is_valid()) {
    LOG(ERROR) << "url_ends_with is not valid: " << replace_url->url_ends_with;
    return false;
  }
  if (!replace_url->new_url.is_valid()) {
    LOG(ERROR) << "new_url is not valid: " << replace_url->new_url;
    return false;
  }
  return true;
}

bool ValidateAppendToQuery(
    const mojom::UrlRequestRewriteAppendToQueryPtr& append_to_query) {
  if (!append_to_query) {
    LOG(ERROR) << "Append to query is missing";
    return false;
  }
  if (append_to_query->query.empty()) {
    LOG(ERROR) << "Append to query is empty";
    return false;
  }
  return true;
}

bool ValidateRewrite(const mojom::UrlRequestActionPtr& action) {
  switch (action->which()) {
    case mojom::UrlRequestAction::Tag::kAddHeaders:
      return ValidateAddHeaders(action->get_add_headers());
    case mojom::UrlRequestAction::Tag::kRemoveHeader:
      return ValidateRemoveHeader(action->get_remove_header());
    case mojom::UrlRequestAction::Tag::kSubstituteQueryPattern:
      return ValidateSubstituteQueryPattern(
          action->get_substitute_query_pattern());
    case mojom::UrlRequestAction::Tag::kReplaceUrl:
      return ValidateReplaceUrl(action->get_replace_url());
    case mojom::UrlRequestAction::Tag::kAppendToQuery:
      return ValidateAppendToQuery(action->get_append_to_query());
    case mojom::UrlRequestAction::Tag::kPolicy:
      return true;
  }
}

}  // namespace

bool ValidateRules(const mojom::UrlRequestRewriteRules* rules) {
  static constexpr std::string_view kWildcard("*.");
  if (!rules)
    return false;
  for (const auto& rule : rules->rules) {
    if (!rule)
      return false;
    if (rule->hosts_filter) {
      if (rule->hosts_filter->empty()) {
        LOG(ERROR) << "Hosts filter is empty";
        return false;
      }
      for (const std::string_view host : *rule->hosts_filter) {
        if (base::StartsWith(host, kWildcard, base::CompareCase::SENSITIVE)) {
          if (!IsValidUrlHost(host.substr(2))) {
            LOG(ERROR) << "Host filter is not valid: " << host;
            return false;
          }
        } else {
          if (!IsValidUrlHost(host)) {
            LOG(ERROR) << "Host filter is not valid: " << host;
            return false;
          }
        }
      }
    }
    if (rule->schemes_filter && rule->schemes_filter->empty()) {
      LOG(ERROR) << "Schemes filter is empty";
      return false;
    }

    if (rule->actions.empty()) {
      // No rewrites, no action = no point!
      LOG(ERROR) << "Actions are empty";
      return false;
    }

    for (const auto& action : rule->actions) {
      if (!action)
        return false;
      if (action->is_policy() && rule->actions.size() > 1)
        // |policy| cannot be combined with other rewrites.
        return false;
      if (!ValidateRewrite(action))
        return false;
    }
  }
  return true;
}

}  // namespace url_rewrite
