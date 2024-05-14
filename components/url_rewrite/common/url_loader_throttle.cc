// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_rewrite/common/url_loader_throttle.h"

#include <string>
#include <string_view>

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/resource_request.h"
#include "url/url_constants.h"

namespace url_rewrite {
namespace {

// Returns a string representing the URL stripped of query and ref.
std::string ClearUrlQueryRef(const GURL& url) {
  GURL::Replacements replacements;
  replacements.ClearQuery();
  replacements.ClearRef();
  return url.ReplaceComponents(replacements).spec();
}

void ApplySubstituteQueryPattern(
    network::ResourceRequest* request,
    const mojom::UrlRequestRewriteSubstituteQueryPatternPtr&
        substitute_query_pattern) {
  std::string url_query = request->url.query();

  base::ReplaceSubstringsAfterOffset(&url_query, 0,
                                     substitute_query_pattern->pattern,
                                     substitute_query_pattern->substitution);

  GURL::Replacements replacements;
  replacements.SetQueryStr(url_query);
  request->url = request->url.ReplaceComponents(replacements);
}

void ApplyReplaceUrl(network::ResourceRequest* request,
                     const mojom::UrlRequestRewriteReplaceUrlPtr& replace_url) {
  if (!base::EndsWith(ClearUrlQueryRef(request->url),
                      replace_url->url_ends_with, base::CompareCase::SENSITIVE))
    return;

  GURL new_url = replace_url->new_url;
  if (new_url.SchemeIs(url::kDataScheme)) {
    request->url = new_url;
    return;
  }

  if (new_url.has_scheme() &&
      new_url.scheme().compare(request->url.scheme()) != 0) {
    // No cross-scheme redirect allowed.
    return;
  }

  GURL::Replacements replacements;
  std::string host = new_url.host();
  replacements.SetHostStr(host);
  std::string port = new_url.port();
  replacements.SetPortStr(port);
  std::string path = new_url.path();
  replacements.SetPathStr(path);

  request->url = request->url.ReplaceComponents(replacements);
}

void ApplyRemoveHeader(
    network::ResourceRequest* request,
    const mojom::UrlRequestRewriteRemoveHeaderPtr& remove_header) {
  std::optional<std::string> query_pattern = remove_header->query_pattern;
  if (query_pattern &&
      request->url.query().find(query_pattern.value()) == std::string::npos) {
    // Per the FIDL API, the header should be removed if there is no query
    // pattern or if the pattern matches. Neither is true here.
    return;
  }

  request->headers.RemoveHeader(remove_header->header_name);
  request->cors_exempt_headers.RemoveHeader(remove_header->header_name);
}

void ApplyAppendToQuery(
    network::ResourceRequest* request,
    const mojom::UrlRequestRewriteAppendToQueryPtr& append_to_query) {
  std::string url_query;
  if (request->url.has_query() && !request->url.query().empty())
    url_query = request->url.query() + "&";
  url_query += append_to_query->query;

  GURL::Replacements replacements;
  replacements.SetQueryStr(url_query);
  request->url = request->url.ReplaceComponents(replacements);
}

bool HostMatches(std::string_view url_host, std::string_view rule_host) {
  const std::string_view kWildcard("*.");
  if (base::StartsWith(rule_host, kWildcard, base::CompareCase::SENSITIVE)) {
    if (base::EndsWith(url_host, rule_host.substr(1),
                       base::CompareCase::SENSITIVE)) {
      return true;
    }

    // Check |url_host| is exactly |rule_host| without the wildcard. i.e. if
    // |rule_host| is "*.test.xyz", check |url_host| is exactly "test.xyz".
    return base::CompareCaseInsensitiveASCII(url_host, rule_host.substr(2)) ==
           0;
  }
  return base::CompareCaseInsensitiveASCII(url_host, rule_host) == 0;
}

// Returns true if the host and scheme filters defined in |rule| match |url|.
bool RuleFiltersMatchUrl(const GURL& url,
                         const mojom::UrlRequestRulePtr& rule) {
  if (rule->hosts_filter) {
    bool found = false;
    for (const std::string_view host : rule->hosts_filter.value()) {
      if ((found = HostMatches(url.host(), host)))
        break;
    }
    if (!found)
      return false;
  }

  if (rule->schemes_filter) {
    bool found = false;
    for (const auto& scheme : rule->schemes_filter.value()) {
      if (url.scheme().compare(scheme) == 0) {
        found = true;
        break;
      }
    }
    if (!found)
      return false;
  }

  return true;
}

// Returns true if |request| is either allowed or left unblocked by any rules.
bool IsRequestAllowed(network::ResourceRequest* request,
                      const mojom::UrlRequestRewriteRulesPtr& rules) {
  for (const auto& rule : rules->rules) {
    if (rule->actions.size() != 1)
      continue;

    if (rule->actions[0]->which() != mojom::UrlRequestAction::Tag::kPolicy)
      continue;

    if (!RuleFiltersMatchUrl(request->url, rule))
      continue;

    switch (rule->actions[0]->get_policy()) {
      case mojom::UrlRequestAccessPolicy::kAllow:
        return true;
      case mojom::UrlRequestAccessPolicy::kDeny:
        return false;
    }
  }

  return true;
}

}  // namespace

URLLoaderThrottle::URLLoaderThrottle(
    scoped_refptr<UrlRequestRewriteRules> rules,
    IsHeaderCorsExemptCallback is_header_cors_exempt_callback)
    : rules_(std::move(rules)),
      is_header_cors_exempt_callback_(
          std::move(is_header_cors_exempt_callback)) {
  DCHECK(rules_);
  DCHECK(is_header_cors_exempt_callback_);
}

URLLoaderThrottle::~URLLoaderThrottle() = default;

void URLLoaderThrottle::DetachFromCurrentSequence() {}

void URLLoaderThrottle::WillStartRequest(network::ResourceRequest* request,
                                         bool* defer) {
  if (!IsRequestAllowed(request, rules_->data)) {
    delegate_->CancelWithError(net::ERR_ABORTED,
                               "Resource load blocked by embedder policy.");
    return;
  }

  for (const auto& rule : rules_->data->rules)
    ApplyRule(request, rule);
  *defer = false;
}

void URLLoaderThrottle::ApplyRule(network::ResourceRequest* request,
                                  const mojom::UrlRequestRulePtr& rule) {
  // Prevent applying rules on redirect navigations.
  if (request->navigation_redirect_chain.size() > 1u)
    return;

  if (!RuleFiltersMatchUrl(request->url, rule))
    return;

  for (const auto& rewrite : rule->actions)
    ApplyRewrite(request, rewrite);
}

void URLLoaderThrottle::ApplyRewrite(
    network::ResourceRequest* request,
    const mojom::UrlRequestActionPtr& rewrite) {
  switch (rewrite->which()) {
    case mojom::UrlRequestAction::Tag::kAddHeaders:
      ApplyAddHeaders(request, rewrite->get_add_headers());
      return;
    case mojom::UrlRequestAction::Tag::kRemoveHeader:
      ApplyRemoveHeader(request, rewrite->get_remove_header());
      return;
    case mojom::UrlRequestAction::Tag::kSubstituteQueryPattern:
      ApplySubstituteQueryPattern(request,
                                  rewrite->get_substitute_query_pattern());
      return;
    case mojom::UrlRequestAction::Tag::kReplaceUrl:
      ApplyReplaceUrl(request, rewrite->get_replace_url());
      return;
    case mojom::UrlRequestAction::Tag::kAppendToQuery:
      ApplyAppendToQuery(request, rewrite->get_append_to_query());
      return;
    case mojom::UrlRequestAction::Tag::kPolicy:
      // "Policy" is interpreted elsewhere; it is a no-op for rewriting.
      return;
  }
  NOTREACHED_IN_MIGRATION();  // Invalid enum value.
}

void URLLoaderThrottle::ApplyAddHeaders(
    network::ResourceRequest* request,
    const mojom::UrlRequestRewriteAddHeadersPtr& add_headers) {
  // Bucket each |header| into the regular/CORS-compliant header list or the
  // CORS-exempt header list.
  for (const auto& header : add_headers->headers) {
    if (request->headers.HasHeader(header->name) ||
        request->cors_exempt_headers.HasHeader(header->name)) {
      // Skip headers already present in the request at this point.
      continue;
    }
    if (is_header_cors_exempt_callback_.Run(header->name)) {
      request->cors_exempt_headers.SetHeader(header->name, header->value);
    } else {
      request->headers.SetHeader(header->name, header->value);
    }
  }
}

}  // namespace url_rewrite
