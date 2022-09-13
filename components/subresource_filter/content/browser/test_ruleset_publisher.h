// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_TEST_RULESET_PUBLISHER_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_TEST_RULESET_PUBLISHER_H_

#include "base/memory/raw_ptr.h"
#include "components/subresource_filter/core/common/test_ruleset_creator.h"

namespace subresource_filter {

class RulesetService;

namespace testing {

// Helper class to create testing rulesets during browser tests, as well as to
// get them indexed and published to renderers by the RulesetService.
class TestRulesetPublisher {
 public:
  explicit TestRulesetPublisher(RulesetService* ruleset_service);

  TestRulesetPublisher(const TestRulesetPublisher&) = delete;
  TestRulesetPublisher& operator=(const TestRulesetPublisher&) = delete;

  ~TestRulesetPublisher();

  // Indexes the |unindexed_ruleset| and publishes it to all renderers
  // via the RulesetService. Spins a nested run loop until done.
  void SetRuleset(const TestRuleset& unindexed_ruleset);

 private:
  raw_ptr<RulesetService> ruleset_service_;
};

}  // namespace testing
}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_TEST_RULESET_PUBLISHER_H_
