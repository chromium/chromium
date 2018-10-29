// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_matcher/url_matcher_factory.h"

#include <algorithm>
#include <cctype>
#include <memory>
#include <utility>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "components/url_matcher/url_matcher_constants.h"
#include "components/url_matcher/url_matcher_helpers.h"
#include "third_party/re2/src/re2/re2.h"

namespace url_matcher {

namespace helpers = url_matcher_helpers;
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

  DISALLOW_COPY_AND_ASSIGN(URLMatcherConditionFactoryMethods);
};

static base::LazyInstance<URLMatcherConditionFactoryMethods>::DestructorAtExit
    g_url_matcher_condition_factory_methods = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
scoped_refptr<URLMatcherConditionSet>
URLMatcherFactory::CreateFromURLFilterDictionary(
    URLMatcherConditionFactory* url_matcher_condition_factory,
    const base::DictionaryValue* url_filter_dict,
    URLMatcherConditionSet::ID id,
    std::string* error) {
  std::unique_ptr<URLMatcherSchemeFilter> url_matcher_schema_filter;
  std::unique_ptr<URLMatcherPortFilter> url_matcher_port_filter;
  URLMatcherConditionSet::Conditions url_matcher_conditions;

  for (base::DictionaryValue::Iterator iter(*url_filter_dict);
       !iter.IsAtEnd(); iter.Advance()) {
    const std::string& condition_attribute_name = iter.key();
    const base::Value& condition_attribute_value = iter.value();
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
                                 std::move(url_matcher_port_filter)));
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
  return std::find_if(str.begin(), str.end(), ::isupper) != str.end();
}

}  // namespace

// static
URLMatcherCondition URLMatcherFactory::CreateURLMatcherCondition(
    URLMatcherConditionFactory* url_matcher_condition_factory,
    const std::string& condition_attribute_name,
    const base::Value* value,
    std::string* error) {
  std::string str_value;
  if (!value->GetAsString(&str_value)) {
    *error = base::StringPrintf(kAttributeExpectedString,
                                condition_attribute_name.c_str());
    return URLMatcherCondition();
  }
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
  if (!helpers::GetAsStringVector(value, &schemas)) {
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
  const base::ListValue* value_list = nullptr;
  if (!value->GetAsList(&value_list)) {
    *error = kInvalidPortRanges;
    return nullptr;
  }

  for (const auto& entry : *value_list) {
    int port = 0;
    const base::ListValue* range = nullptr;
    if (entry.GetAsInteger(&port)) {
      ranges.push_back(URLMatcherPortFilter::CreateRange(port));
    } else if (entry.GetAsList(&range)) {
      int from = 0, to = 0;
      if (range->GetSize() != 2u ||
          !range->GetInteger(0, &from) ||
          !range->GetInteger(1, &to)) {
        *error = kInvalidPortRanges;
        return nullptr;
      }
      ranges.push_back(URLMatcherPortFilter::CreateRange(from, to));
    } else {
      *error = kInvalidPortRanges;
      return nullptr;
    }
  }

  return std::make_unique<URLMatcherPortFilter>(ranges);
}

}  // namespace url_matcher
