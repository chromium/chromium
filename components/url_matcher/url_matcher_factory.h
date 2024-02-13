// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_URL_MATCHER_URL_MATCHER_FACTORY_H_
#define COMPONENTS_URL_MATCHER_URL_MATCHER_FACTORY_H_

#include <memory>
#include <string>

#include "base/values.h"
#include "components/url_matcher/url_matcher.h"
#include "components/url_matcher/url_matcher_export.h"

namespace url_matcher {

class URL_MATCHER_EXPORT URLMatcherFactory {
 public:
  URLMatcherFactory() = delete;
  URLMatcherFactory(const URLMatcherFactory&) = delete;
  URLMatcherFactory& operator=(const URLMatcherFactory&) = delete;

  // Creates a URLMatcherConditionSet from a UrlFilter dictionary as defined in
  // the declarative API. |url_fetcher_dict| contains the dictionary passed
  // by the extension, |id| is the identifier assigned to the created
  // URLMatcherConditionSet. In case of an error, |error| is set to contain
  // an error message.
  //
  // Note: In case this function fails or if you don't register the
  // URLMatcherConditionSet to the URLMatcher, you need to call
  // URLMatcher::ClearUnusedConditionSets() on the URLMatcher that owns this
  // URLMatcherFactory. Otherwise you leak memory.
  static scoped_refptr<URLMatcherConditionSet> CreateFromURLFilterDictionary(
      URLMatcherConditionFactory* url_matcher_condition_factory,
      const base::Value::Dict& url_filter_dict,
      base::MatcherStringPattern::ID id,
      std::string* error);

 private:
  // Returns whether a condition attribute with name |condition_attribute_name|
  // needs to be handled by the URLMatcher.
  static bool IsURLMatcherConditionAttribute(
      const std::string& condition_attribute_name);

  // Factory method of for URLMatcherConditions.
  static URLMatcherCondition CreateURLMatcherCondition(
      URLMatcherConditionFactory* url_matcher_condition_factory,
      const std::string& condition_attribute_name,
      const base::Value* value,
      std::string* error);

  static std::unique_ptr<URLMatcherSchemeFilter> CreateURLMatcherScheme(
      const base::Value* value,
      std::string* error);

  static std::unique_ptr<URLMatcherPortFilter> CreateURLMatcherPorts(
      const base::Value* value,
      std::string* error);

  static std::unique_ptr<URLMatcherCidrBlockFilter> CreateURLMatcherCidrBlocks(
      const base::Value* value,
      std::string* error);
};

}  // namespace url_matcher

#endif  // COMPONENTS_URL_MATCHER_URL_MATCHER_FACTORY_H_
