// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/url_rewrite/url_request_rewrite_type_converters.h"

#include <string_view>

#include "base/strings/strcat.h"
#include "base/strings/string_util.h"

namespace {

std::string NormalizeHost(std::string_view host) {
  return GURL(base::StrCat({url::kHttpScheme, "://", host})).host();
}

}  // namespace

namespace mojo {

template <>
struct TypeConverter<url_rewrite::mojom::UrlRequestRewriteAddHeadersPtr,
                     cast::v2::UrlRequestRewriteAddHeaders> {
  static url_rewrite::mojom::UrlRequestRewriteAddHeadersPtr Convert(
      const cast::v2::UrlRequestRewriteAddHeaders& input) {
    url_rewrite::mojom::UrlRequestRewriteAddHeadersPtr add_headers =
        url_rewrite::mojom::UrlRequestRewriteAddHeaders::New();
    for (const auto& header : input.headers()) {
      url_rewrite::mojom::UrlHeaderPtr url_header =
          url_rewrite::mojom::UrlHeader::New(header.name(), header.value());
      add_headers->headers.push_back(std::move(url_header));
    }
    return add_headers;
  }
};

template <>
struct TypeConverter<url_rewrite::mojom::UrlRequestRewriteRemoveHeaderPtr,
                     cast::v2::UrlRequestRewriteRemoveHeader> {
  static url_rewrite::mojom::UrlRequestRewriteRemoveHeaderPtr Convert(
      const cast::v2::UrlRequestRewriteRemoveHeader& input) {
    url_rewrite::mojom::UrlRequestRewriteRemoveHeaderPtr remove_header =
        url_rewrite::mojom::UrlRequestRewriteRemoveHeader::New();
    if (!input.query_pattern().empty())
      remove_header->query_pattern = std::make_optional(input.query_pattern());
    if (!input.header_name().empty())
      remove_header->header_name = input.header_name();
    return remove_header;
  }
};

template <>
struct TypeConverter<
    url_rewrite::mojom::UrlRequestRewriteSubstituteQueryPatternPtr,
    cast::v2::UrlRequestRewriteSubstituteQueryPattern> {
  static url_rewrite::mojom::UrlRequestRewriteSubstituteQueryPatternPtr Convert(
      const cast::v2::UrlRequestRewriteSubstituteQueryPattern& input) {
    url_rewrite::mojom::UrlRequestRewriteSubstituteQueryPatternPtr
        substitute_query_pattern =
            url_rewrite::mojom::UrlRequestRewriteSubstituteQueryPattern::New();
    if (!input.pattern().empty())
      substitute_query_pattern->pattern = input.pattern();
    if (!input.substitution().empty())
      substitute_query_pattern->substitution = input.substitution();
    return substitute_query_pattern;
  }
};

template <>
struct TypeConverter<url_rewrite::mojom::UrlRequestRewriteReplaceUrlPtr,
                     cast::v2::UrlRequestRewriteReplaceUrl> {
  static url_rewrite::mojom::UrlRequestRewriteReplaceUrlPtr Convert(
      const cast::v2::UrlRequestRewriteReplaceUrl& input) {
    url_rewrite::mojom::UrlRequestRewriteReplaceUrlPtr replace_url =
        url_rewrite::mojom::UrlRequestRewriteReplaceUrl::New();
    if (!input.url_ends_with().empty())
      replace_url->url_ends_with = input.url_ends_with();
    if (!input.new_url().empty())
      replace_url->new_url = GURL(input.new_url());
    return replace_url;
  }
};

template <>
struct TypeConverter<url_rewrite::mojom::UrlRequestRewriteAppendToQueryPtr,
                     cast::v2::UrlRequestRewriteAppendToQuery> {
  static url_rewrite::mojom::UrlRequestRewriteAppendToQueryPtr Convert(
      const cast::v2::UrlRequestRewriteAppendToQuery& input) {
    url_rewrite::mojom::UrlRequestRewriteAppendToQueryPtr append_to_query =
        url_rewrite::mojom::UrlRequestRewriteAppendToQuery::New();
    if (!input.query().empty())
      append_to_query->query = input.query();
    return append_to_query;
  }
};

// Returns nullptr if conversion is not possible.
template <>
struct TypeConverter<url_rewrite::mojom::UrlRequestActionPtr,
                     cast::v2::UrlRequestRewriteRule::UrlRequestAction> {
  static url_rewrite::mojom::UrlRequestActionPtr Convert(
      const cast::v2::UrlRequestRewriteRule::UrlRequestAction& input) {
    // The converter should never get called if policy action is unspecified.
    DCHECK(input != cast::v2::UrlRequestRewriteRule::ACTION_UNSPECIFIED);
    switch (input) {
      case cast::v2::UrlRequestRewriteRule::ALLOW:
        return url_rewrite::mojom::UrlRequestAction::NewPolicy(
            mojo::ConvertTo<url_rewrite::mojom::UrlRequestAccessPolicy>(
                url_rewrite::mojom::UrlRequestAccessPolicy::kAllow));
      case cast::v2::UrlRequestRewriteRule::DENY:
        return url_rewrite::mojom::UrlRequestAction::NewPolicy(
            mojo::ConvertTo<url_rewrite::mojom::UrlRequestAccessPolicy>(
                url_rewrite::mojom::UrlRequestAccessPolicy::kDeny));
      default:
        // Cannot convert the gRPC policy action to a mojo counterpart.
        return nullptr;
    }
  }
};

// Returns nullptr if conversion is not possible.
template <>
struct TypeConverter<url_rewrite::mojom::UrlRequestActionPtr,
                     cast::v2::UrlRequestRewrite> {
  static url_rewrite::mojom::UrlRequestActionPtr Convert(
      const cast::v2::UrlRequestRewrite& input) {
    switch (input.action_case()) {
      case cast::v2::UrlRequestRewrite::kAddHeaders:
        return url_rewrite::mojom::UrlRequestAction::NewAddHeaders(
            mojo::ConvertTo<url_rewrite::mojom::UrlRequestRewriteAddHeadersPtr>(
                input.add_headers()));
      case cast::v2::UrlRequestRewrite::kRemoveHeader:
        return url_rewrite::mojom::UrlRequestAction::NewRemoveHeader(
            mojo::ConvertTo<
                url_rewrite::mojom::UrlRequestRewriteRemoveHeaderPtr>(
                input.remove_header()));
      case cast::v2::UrlRequestRewrite::kSubstituteQueryPattern:
        return url_rewrite::mojom::UrlRequestAction::NewSubstituteQueryPattern(
            mojo::ConvertTo<
                url_rewrite::mojom::UrlRequestRewriteSubstituteQueryPatternPtr>(
                input.substitute_query_pattern()));
      case cast::v2::UrlRequestRewrite::kReplaceUrl:
        return url_rewrite::mojom::UrlRequestAction::NewReplaceUrl(
            mojo::ConvertTo<url_rewrite::mojom::UrlRequestRewriteReplaceUrlPtr>(
                input.replace_url()));
      case cast::v2::UrlRequestRewrite::kAppendToQuery:
        return url_rewrite::mojom::UrlRequestAction::NewAppendToQuery(
            mojo::ConvertTo<
                url_rewrite::mojom::UrlRequestRewriteAppendToQueryPtr>(
                input.append_to_query()));
      case cast::v2::UrlRequestRewrite::ACTION_NOT_SET:
        // Cannot convert the gRPC rewrite rule to a mojo counterpart.
        return nullptr;
    }
  }
};

// Returns nullptr if conversion is not possible.
template <>
struct TypeConverter<url_rewrite::mojom::UrlRequestRulePtr,
                     cast::v2::UrlRequestRewriteRule> {
  static url_rewrite::mojom::UrlRequestRulePtr Convert(
      const cast::v2::UrlRequestRewriteRule& input) {
    url_rewrite::mojom::UrlRequestRulePtr rule =
        url_rewrite::mojom::UrlRequestRule::New();

    if (!input.host_filters().empty()) {
      // Convert host names in case they contain non-ASCII characters.
      const std::string_view kWildcard("*.");

      std::vector<std::string> hosts;
      for (std::string_view host : input.host_filters()) {
        if (base::StartsWith(host, kWildcard, base::CompareCase::SENSITIVE)) {
          hosts.push_back(
              base::StrCat({kWildcard, NormalizeHost(host.substr(2))}));
        } else {
          hosts.push_back(NormalizeHost(host));
        }
      }
      rule->hosts_filter = std::move(hosts);
    }

    if (!input.scheme_filters().empty())
      rule->schemes_filter.emplace(input.scheme_filters().begin(),
                                   input.scheme_filters().end());

    // Convert the rewrite rules.
    for (const cast::v2::UrlRequestRewrite& grpc_rewrite : input.rewrites()) {
      auto action = mojo::ConvertTo<url_rewrite::mojom::UrlRequestActionPtr>(
          grpc_rewrite);
      if (!action) {
        // Conversion to Mojo failed.
        return nullptr;
      }
      rule->actions.push_back(std::move(action));
    }

    // Convert the action policy.
    if (input.action() != cast::v2::UrlRequestRewriteRule::ACTION_UNSPECIFIED) {
      // Convert the action policy.
      auto policy = mojo::ConvertTo<url_rewrite::mojom::UrlRequestActionPtr>(
          input.action());
      if (!policy) {
        // Conversion to Mojo failed.
        return nullptr;
      }
      rule->actions.push_back(std::move(policy));
    }

    return rule;
  }
};

url_rewrite::mojom::UrlRequestRewriteRulesPtr
TypeConverter<url_rewrite::mojom::UrlRequestRewriteRulesPtr,
              cast::v2::UrlRequestRewriteRules>::
    Convert(const cast::v2::UrlRequestRewriteRules& input) {
  url_rewrite::mojom::UrlRequestRewriteRulesPtr rules =
      url_rewrite::mojom::UrlRequestRewriteRules::New();
  for (const auto& rule : input.rules()) {
    auto mojo_rule =
        mojo::ConvertTo<url_rewrite::mojom::UrlRequestRulePtr>(rule);
    if (!mojo_rule) {
      // Conversion to Mojo failed.
      return nullptr;
    }
    rules->rules.push_back(std::move(mojo_rule));
  }
  return rules;
}

}  // namespace mojo
