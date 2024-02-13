// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_matcher/url_matcher_factory.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/lazy_instance.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "components/url_matcher/url_matcher_constants.h"
#include "components/url_matcher/url_util.h"
#include "third_party/re2/src/re2/re2.h"

namespace url_matcher {

namespace keys = url_matcher_constants;

namespace {

// Error messages:
const char kInvalidPortRanges[] = "Invalid port ranges in UrlFilter.";
const char kVectorOfStringsExpected[] =
    "UrlFilter attribute '%s' expected a vector of strings as parameter.";
const char kUnknownURLFilterAttribute[] =
    "Unknown attribute '%s' in UrlFilter.";
const char kAttributeExpectedString[] =
    "UrlFilter attribute '%s' expected a string value.";
const char kUnparseableRegexString[] =
    "Could not parse regular expression '%s': %s";
const char kLowerCaseExpected[] = "%s values need to be in lower case.";
const char kInvalidCidrBlocks[] = "Invalid CIDR blocks in UrlFilter.";

// Registry for all factory methods of URLMatcherConditionFactory
// that allows translating string literals from the extension API into
// the corresponding factory method to be called.
class URLMatcherConditionFactoryMethods {
 public:
  URLMatcherConditionFactoryMethods() {
    typedef URLMatcherConditionFactory F;
    factory_methods_[keys::kHostContainsKey] = &F::CreateHostContainsCondition;
    factory_methods_[keys::kHostEqualsKey] = &F::CreateHostEqualsCondition;
    factory_methods_[keys::kHostPrefixKey] = &F::CreateHostPrefixCondition;
    factory_methods_[keys::kHostSuffixKey] = &F::CreateHostSuffixCondition;
    factory_methods_[keys::kOriginAndPathMatchesKey] =
        &F::CreateOriginAndPathMatchesCondition;
    factory_methods_[keys::kPathContainsKey] = &F::CreatePathContainsCondition;
    factory_methods_[keys::kPathEqualsKey] = &F::CreatePathEqualsCondition;
    factory_methods_[keys::kPathPrefixKey] = &F::CreatePathPrefixCondition;
    factory_methods_[keys::kPathSuffixKey] = &F::CreatePathSuffixCondition;
    factory_methods_[keys::kQueryContainsKey] =
        &F::CreateQueryContainsCondition;
    factory_methods_[keys::kQueryEqualsKey] = &F::CreateQueryEqualsCondition;
    factory_methods_[keys::kQueryPrefixKey] = &F::CreateQueryPrefixCondition;
    factory_methods_[keys::kQuerySuffixKey] = &F::CreateQuerySuffixCondition;
    factory_methods_[keys::kURLContainsKey] = &F::CreateURLContainsCondition;
    factory_methods_[keys::kURLEqualsKey] = &F::CreateURLEqualsCondition;
    factory_methods_[keys::kURLPrefixKey] = &F::CreateURLPrefixCondition;
    factory_methods_[keys::kURLSuffixKey] = &F::CreateURLSuffixCondition;
    factory_methods_[keys::kURLMatchesKey] = &F::CreateURLMatchesCondition;
  }

  URLMatcherConditionFactoryMethods(const URLMatcherConditionFactoryMethods&) =
      delete;
  URLMatcherConditionFactoryMethods& operator=(
      const URLMatcherConditionFactoryMethods&) = delete;

  // Returns whether a factory method for the specified |pattern_type| (e.g.
  // "host_suffix") is known.
  bool Contains(const std::string& pattern_type) const {
    return factory_methods_.find(pattern_type) != factory_methods_.end();
  }

  // Creates a URLMatcherCondition instance from |url_matcher_condition_factory|
  // of the given |pattern_type| (e.g. "host_suffix") for the given
  // |pattern_value| (e.g. "example.com").
  // The |pattern_type| needs to be known to this class (see Contains()) or
  // a CHECK is triggered.
  URLMatcherCondition Call(
      URLMatcherConditionFactory* url_matcher_condition_factory,
      const std::string& pattern_type,
      const std::string& pattern_value) const {
    auto i = factory_methods_.find(pattern_type);
    CHECK(i != factory_methods_.end());
    const FactoryMethod& method = i->second;
    return (url_matcher_condition_factory->*method)(pattern_value);
  }

 private:
  typedef URLMatcherCondition
      (URLMatcherConditionFactory::* FactoryMethod)
      (const std::string& prefix);
  typedef std::map<std::string, FactoryMethod> FactoryMethods;

  FactoryMethods factory_methods_;
};

static base::LazyInstance<URLMatcherConditionFactoryMethods>::DestructorAtExit
    g_url_matcher_condition_factory_methods = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
scoped_refptr<URLMatcherConditionSet>
URLMatcherFactory::CreateFromURLFilterDictionary(
    URLMatcherConditionFactory* url_matcher_condition_factory,
    const base::Value::Dict& url_filter_dict,
    base::MatcherStringPattern::ID id,
    std::string* error) {
  std::unique_ptr<URLMatcherSchemeFilter> url_matcher_schema_filter;
  std::unique_ptr<URLMatcherPortFilter> url_matcher_port_filter;
  std::unique_ptr<URLMatcherCidrBlockFilter> url_matcher_cidr_block_filter;
  URLMatcherConditionSet::Conditions url_matcher_conditions;

  for (const auto iter : url_filter_dict) {
    const std::string& condition_attribute_name = iter.first;
    const base::Value& condition_attribute_value = iter.second;
    if (IsURLMatcherConditionAttribute(condition_attribute_name)) {
      // Handle {host, path, ...}{Prefix, Suffix, Contains, Equals}.
      URLMatcherCondition url_matcher_condition =
          CreateURLMatcherCondition(
              url_matcher_condition_factory,
              condition_attribute_name,
              &condition_attribute_value,
              error);
      if (!error->empty())
        return scoped_refptr<URLMatcherConditionSet>(nullptr);
      url_matcher_conditions.insert(url_matcher_condition);
    } else if (condition_attribute_name == keys::kSchemesKey) {
      // Handle scheme.
      url_matcher_schema_filter = CreateURLMatcherScheme(
          &condition_attribute_value, error);
      if (!error->empty())
        return scoped_refptr<URLMatcherConditionSet>(nullptr);
    } else if (condition_attribute_name == keys::kPortsKey) {
      // Handle ports.
      url_matcher_port_filter = CreateURLMatcherPorts(
          &condition_attribute_value, error);
      if (!error->empty())
        return scoped_refptr<URLMatcherConditionSet>(nullptr);
    } else if (condition_attribute_name == keys::kCidrBlocksKey) {
      // Handle CIDR blocks.
      url_matcher_cidr_block_filter =
          CreateURLMatcherCidrBlocks(&condition_attribute_value, error);
      if (!error->empty()) {
        return scoped_refptr<URLMatcherConditionSet>(nullptr);
      }
    } else {
      // Handle unknown attributes.
      *error = base::StringPrintf(kUnknownURLFilterAttribute,
                                  condition_attribute_name.c_str());
      return scoped_refptr<URLMatcherConditionSet>(nullptr);
    }
  }

  // As the URL is the preliminary matching criterion that triggers the tests
  // for the remaining condition attributes, we insert an empty URL match if
  // no other url match conditions were specified. Such an empty URL is always
  // matched.
  if (url_matcher_conditions.empty()) {
    url_matcher_conditions.insert(
        url_matcher_condition_factory->CreateHostPrefixCondition(
            std::string()));
  }

  scoped_refptr<URLMatcherConditionSet> url_matcher_condition_set(
      new URLMatcherConditionSet(id, url_matcher_conditions,
                                 std::move(url_matcher_schema_filter),
                                 std::move(url_matcher_port_filter),
                                 std::move(url_matcher_cidr_block_filter)));
  return url_matcher_condition_set;
}

// static
bool URLMatcherFactory::IsURLMatcherConditionAttribute(
    const std::string& condition_attribute_name) {
  return g_url_matcher_condition_factory_methods.Get().Contains(
      condition_attribute_name);
}

namespace {

// Returns true if some alphabetic characters in this string are upper case.
bool ContainsUpperCase(const std::string& str) {
  return base::ranges::any_of(str, ::isupper);
}

}  // namespace

// static
URLMatcherCondition URLMatcherFactory::CreateURLMatcherCondition(
    URLMatcherConditionFactory* url_matcher_condition_factory,
    const std::string& condition_attribute_name,
    const base::Value* value,
    std::string* error) {
  if (!value->is_string()) {
    *error = base::StringPrintf(kAttributeExpectedString,
                                condition_attribute_name.c_str());
    return URLMatcherCondition();
  }
  const std::string& str_value = value->GetString();
  if (condition_attribute_name == keys::kHostContainsKey ||
      condition_attribute_name == keys::kHostPrefixKey ||
      condition_attribute_name == keys::kHostSuffixKey ||
      condition_attribute_name == keys::kHostEqualsKey) {
    if (ContainsUpperCase(str_value)) {
      *error = base::StringPrintf(kLowerCaseExpected, "Host");
      return URLMatcherCondition();
    }
  }

  // Test regular expressions for validity.
  if (condition_attribute_name == keys::kURLMatchesKey ||
      condition_attribute_name == keys::kOriginAndPathMatchesKey) {
    re2::RE2 regex(str_value);
    if (!regex.ok()) {
      *error = base::StringPrintf(
          kUnparseableRegexString, str_value.c_str(), regex.error().c_str());
      return URLMatcherCondition();
    }
  }
  return g_url_matcher_condition_factory_methods.Get().Call(
      url_matcher_condition_factory, condition_attribute_name, str_value);
}

// static
std::unique_ptr<URLMatcherSchemeFilter>
URLMatcherFactory::CreateURLMatcherScheme(const base::Value* value,
                                          std::string* error) {
  std::vector<std::string> schemas;
  if (!util::GetAsStringVector(value, &schemas)) {
    *error = base::StringPrintf(kVectorOfStringsExpected, keys::kSchemesKey);
    return nullptr;
  }
  for (std::vector<std::string>::const_iterator it = schemas.begin();
       it != schemas.end(); ++it) {
    if (ContainsUpperCase(*it)) {
      *error = base::StringPrintf(kLowerCaseExpected, "Scheme");
      return nullptr;
    }
  }
  return std::make_unique<URLMatcherSchemeFilter>(schemas);
}

// static
std::unique_ptr<URLMatcherPortFilter> URLMatcherFactory::CreateURLMatcherPorts(
    const base::Value* value,
    std::string* error) {
  std::vector<URLMatcherPortFilter::Range> ranges;
  if (!value->is_list()) {
    *error = kInvalidPortRanges;
    return nullptr;
  }
  const base::Value::List& value_list = value->GetList();

  for (const auto& entry : value_list) {
    if (entry.is_int()) {
      ranges.push_back(URLMatcherPortFilter::CreateRange(entry.GetInt()));
    } else if (entry.is_list()) {
      const base::Value::List& entry_list = entry.GetList();
      if (entry_list.size() != 2u || !entry_list[0].is_int() ||
          !entry_list[1].is_int()) {
        *error = kInvalidPortRanges;
        return nullptr;
      }
      int from = entry_list[0].GetInt();
      int to = entry_list[1].GetInt();
      ranges.push_back(URLMatcherPortFilter::CreateRange(from, to));
    } else {
      *error = kInvalidPortRanges;
      return nullptr;
    }
  }

  return std::make_unique<URLMatcherPortFilter>(ranges);
}

// static
std::unique_ptr<URLMatcherCidrBlockFilter>
URLMatcherFactory::CreateURLMatcherCidrBlocks(const base::Value* value,
                                              std::string* error) {
  std::vector<URLMatcherCidrBlockFilter::CidrBlock> cidr_blocks;
  if (!value->is_list()) {
    *error = kInvalidCidrBlocks;
    return nullptr;
  }

  cidr_blocks.reserve(value->GetList().size());
  for (const auto& entry : value->GetList()) {
    if (!entry.is_string()) {
      *error = kInvalidCidrBlocks;
      return nullptr;
    }

    base::expected<URLMatcherCidrBlockFilter::CidrBlock, std::string>
        cidr_block =
            URLMatcherCidrBlockFilter::CreateCidrBlock(entry.GetString());
    if (!cidr_block.has_value()) {
      *error = cidr_block.error();
      return nullptr;
    }

    cidr_blocks.push_back(std::move(*cidr_block));
  }

  return std::make_unique<URLMatcherCidrBlockFilter>(std::move(cidr_blocks));
}

}  // namespace url_matcher
