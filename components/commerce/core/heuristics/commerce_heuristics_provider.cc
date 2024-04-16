// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/heuristics/commerce_heuristics_provider.h"

#include <set>

#include "base/feature_list.h"
#include "base/json/json_reader.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "build/buildflag.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/commerce_heuristics_data.h"
#include "components/commerce/core/commerce_heuristics_data_metrics_helper.h"
#include "components/grit/components_resources.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/base/resource/resource_bundle.h"

namespace commerce_heuristics {

namespace {

constexpr unsigned kLengthLimit = 4096;
std::string eTLDPlusOne(const GURL& url) {
  return net::registry_controlled_domains::GetDomainAndRegistry(
      url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

const std::map<std::string, std::string>& GetCartPatternMapping() {
  static base::NoDestructor<std::map<std::string, std::string>> pattern_map([] {
    base::Value json(base::JSONReader::Read(
                         commerce::kCartPatternMapping.Get().empty()
                             ? ui::ResourceBundle::GetSharedInstance()
                                   .LoadDataResourceString(
                                       IDR_CART_DOMAIN_CART_URL_REGEX_JSON)
                             : commerce::kCartPatternMapping.Get())
                         .value());
    DCHECK(json.is_dict());
    std::map<std::string, std::string> map;
    for (auto&& item : json.GetDict()) {
      map.insert({std::move(item.first), std::move(item.second).TakeString()});
    }
    return map;
  }());
  return *pattern_map;
}

const std::map<std::string, std::string>& GetCheckoutPatternMapping() {
  static base::NoDestructor<std::map<std::string, std::string>> pattern_map([] {
    base::Value json(
        base::JSONReader::Read(
            commerce::kCheckoutPatternMapping.Get().empty()
                ? ui::ResourceBundle::GetSharedInstance()
                      .LoadDataResourceString(
                          IDR_CHECKOUT_URL_REGEX_DOMAIN_MAPPING_JSON)
                : commerce::kCheckoutPatternMapping.Get())
            .value());
    DCHECK(json.is_dict());
    std::map<std::string, std::string> map;
    for (auto&& item : json.GetDict()) {
      map.insert({std::move(item.first), std::move(item.second).TakeString()});
    }
    return map;
  }());
  return *pattern_map;
}

const re2::RE2* GetVisitCartPattern(const GURL& url) {
  std::string domain = eTLDPlusOne(url);
  auto* pattern_from_component =
      commerce_heuristics::CommerceHeuristicsData::GetInstance()
          .GetCartPageURLPatternForDomain(domain);
  if (pattern_from_component &&
      commerce::kCartPatternMapping.Get() ==
          commerce::kCartPatternMapping.default_value) {
    return pattern_from_component;
  }
  const std::map<std::string, std::string>& cart_string_map =
      GetCartPatternMapping();
  static base::NoDestructor<std::map<std::string, std::unique_ptr<re2::RE2>>>
      cart_regex_map;
  static re2::RE2::Options options;
  options.set_case_sensitive(false);
  if (cart_string_map.find(domain) == cart_string_map.end()) {
    auto* global_pattern_from_component =
        commerce_heuristics::CommerceHeuristicsData::GetInstance()
            .GetCartPageURLPattern();
    if (global_pattern_from_component &&
        commerce::kCartPattern.Get() == commerce::kCartPattern.default_value) {
      return global_pattern_from_component;
    }
    static base::NoDestructor<re2::RE2> instance(commerce::kCartPattern.Get(),
                                                 options);
    return instance.get();
  }
  if (cart_regex_map->find(domain) == cart_regex_map->end()) {
    cart_regex_map->insert({domain, std::make_unique<re2::RE2>(
                                        cart_string_map.at(domain), options)});
  }
  return cart_regex_map->at(domain).get();
}

// TODO(crbug.com/40163450): cover more shopping sites.
const re2::RE2* GetVisitCheckoutPattern(const GURL& url) {
  std::string domain = eTLDPlusOne(url);
  auto* pattern_from_component =
      commerce_heuristics::CommerceHeuristicsData::GetInstance()
          .GetCheckoutPageURLPatternForDomain(domain);
  if (pattern_from_component &&
      commerce::kCheckoutPatternMapping.Get() ==
          commerce::kCheckoutPatternMapping.default_value) {
    return pattern_from_component;
  }
  const std::map<std::string, std::string>& checkout_string_map =
      GetCheckoutPatternMapping();
  static base::NoDestructor<std::map<std::string, std::unique_ptr<re2::RE2>>>
      checkout_regex_map;
  static re2::RE2::Options options;
  options.set_case_sensitive(false);
  if (checkout_string_map.find(domain) == checkout_string_map.end()) {
    auto* global_pattern_from_component =
        commerce_heuristics::CommerceHeuristicsData::GetInstance()
            .GetCheckoutPageURLPattern();
    if (global_pattern_from_component &&
        commerce::kCheckoutPattern.Get() ==
            commerce::kCheckoutPattern.default_value) {
      CommerceHeuristicsDataMetricsHelper::
          RecordCheckoutURLGeneralPatternSource(
              CommerceHeuristicsDataMetricsHelper::HeuristicsSource::
                  FROM_COMPONENT);
      return global_pattern_from_component;
    }
    static base::NoDestructor<re2::RE2> instance(
        commerce::kCheckoutPattern.Get(), options);
    CommerceHeuristicsDataMetricsHelper::RecordCheckoutURLGeneralPatternSource(
        CommerceHeuristicsDataMetricsHelper::HeuristicsSource::
            FROM_FEATURE_PARAMETER);
    return instance.get();
  }
  if (checkout_regex_map->find(domain) == checkout_regex_map->end()) {
    checkout_regex_map->insert(
        {domain,
         std::make_unique<re2::RE2>(checkout_string_map.at(domain), options)});
  }
  return checkout_regex_map->at(domain).get();
}

std::string CanonicalURL(const GURL& url) {
  return base::JoinString({url.scheme_piece(), "://", url.host_piece(),
                           url.path_piece().substr(0, kLengthLimit)},
                          "");
}

}  // namespace

bool IsVisitCart(const GURL& url) {
  auto* pattern = GetVisitCartPattern(url);
  if (!pattern)
    return false;
  return RE2::PartialMatch(CanonicalURL(url).substr(0, kLengthLimit), *pattern);
}

bool IsVisitCheckout(const GURL& url) {
  auto* pattern = GetVisitCheckoutPattern(url);
  if (!pattern)
    return false;
  return RE2::PartialMatch(url.spec().substr(0, kLengthLimit), *pattern);
}

bool IsAddToCartButtonSpec(int height, int width) {
  if (height > width)
    return false;
  int limit_height = commerce::kAddToCartButtonHeightLimit.Get();
  int limit_width = commerce::kAddToCartButtonWidthLimit.Get();
  if (width > limit_width || height > limit_height) {
    return false;
  }
  return true;
}

bool IsAddToCartButtonTag(const std::string& tag) {
  static base::NoDestructor<std::set<std::string>> set([] {
    std::vector<std::string> tags =
        base::SplitString(commerce::kAddToCartButtonTagPattern.Get(), ",",
                          base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    std::set<std::string> set(tags.begin(), tags.end());
    return set;
  }());

  return set->find(tag) != set->end();
}

bool IsAddToCartButtonText(const std::string& text) {
  static re2::RE2::Options options;
  options.set_case_sensitive(false);
  static base::NoDestructor<re2::RE2> instance(
      commerce::kAddToCartButtonTextPattern.Get(), options);
  return RE2::PartialMatch(text.substr(0, kLengthLimit), *instance);
}

bool ShouldUseDOMBasedHeuristics(const GURL& url) {
  if (!base::FeatureList::IsEnabled(commerce::kChromeCartDomBasedHeuristics)) {
    return false;
  }
  static re2::RE2::Options options;
  options.set_case_sensitive(false);
  static base::NoDestructor<re2::RE2> instance(
      commerce::kSkipHeuristicsDomainPattern.Get(), options);
  return !RE2::PartialMatch(eTLDPlusOne(url), *instance);
}

}  // namespace commerce_heuristics
