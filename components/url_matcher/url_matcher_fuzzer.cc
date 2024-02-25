// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <fuzzer/FuzzedDataProvider.h>
#include <string>
#include "base/at_exit.h"
#include "base/i18n/icu_util.h"
#include "base/logging.h"

#include "components/url_matcher/url_matcher.h"
#include "url/gurl.h"

namespace url_matcher {
class Environment {
 public:
  Environment() {
    logging::SetMinLogLevel(logging::LOGGING_FATAL);
    CHECK(base::i18n::InitializeICU());
  }
  base::AtExitManager at_exit_manager;
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment env;
  FuzzedDataProvider provider(data, size);
  const base::MatcherStringPattern::ID kConditionSetId =
      provider.ConsumeIntegralInRange<int>(1, 256);

  // Fuzzing GURLCharacter Set
  GURL url(provider.ConsumeRandomLengthString(size));
  if (!url.is_valid() || url.is_empty()) {
    return -1;
  }
  URLMatcher matcher;
  URLMatcherConditionFactory* factory = matcher.condition_factory();

  URLMatcherConditionSet::Conditions conditions;
  // Fuzzed Conditions
  conditions.insert(factory->CreateHostSuffixCondition(
      provider.ConsumeRandomLengthString(size)));
  conditions.insert(factory->CreatePathContainsCondition(
      provider.ConsumeRemainingBytesAsString()));
  // Regex for any URL, valid RE2 can be fuzzed
  conditions.insert(factory->CreateURLMatchesCondition(
      "(https:\\/\\/www\\.|http:\\/\\/www\\.|https:\\/\\/|http:\\/\\/"
      ")?[a-zA-Z0-9]{2,}("
      "\\.[a-zA-Z0-9]{2,})(\\.[a-zA-Z0-9]{2,})?"));

  // useful conditions from unittests
  conditions.insert(factory->CreateHostContainsCondition("www."));
  conditions.insert(factory->CreateHostContainsCondition("com"));
  conditions.insert(factory->CreatePathEqualsCondition("/webhp"));
  conditions.insert(factory->CreatePathContainsCondition("/we"));
  conditions.insert(factory->CreatePathContainsCondition("hp"));
  conditions.insert(factory->CreateQueryEqualsCondition("test=val&a=b"));
  conditions.insert(factory->CreateQueryContainsCondition("test=v"));
  conditions.insert(factory->CreateQuerySuffixCondition("l&a=b"));

  URLMatcherConditionSet::Vector insert;
  insert.push_back(base::MakeRefCounted<URLMatcherConditionSet>(kConditionSetId,
                                                                conditions));
  matcher.AddConditionSets(insert);
  matcher.MatchURL(url);
  return 0;
}
}  // namespace url_matcher
