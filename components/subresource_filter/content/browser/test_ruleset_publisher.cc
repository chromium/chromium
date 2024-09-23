// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/test_ruleset_publisher.h"

#include "base/functional/callback.h"
#include "base/hash/hash.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "components/subresource_filter/content/shared/browser/ruleset_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_filter {
namespace testing {

namespace {

class RulesetDistributionListener {
 public:
  explicit RulesetDistributionListener(RulesetService* service)
      : service_(service) {
    service_->SetRulesetPublishedCallbackForTesting(run_loop_.QuitClosure());
  }

  RulesetDistributionListener(const RulesetDistributionListener&) = delete;
  RulesetDistributionListener& operator=(const RulesetDistributionListener&) =
      delete;

  ~RulesetDistributionListener() {
    service_->SetRulesetPublishedCallbackForTesting(base::OnceClosure());
  }

  void AwaitDistribution() { run_loop_.Run(); }

 private:
  raw_ptr<RulesetService> service_;
  base::RunLoop run_loop_;
};

}  // namespace

TestRulesetPublisher::TestRulesetPublisher(RulesetService* ruleset_service)
    : ruleset_service_(ruleset_service) {}

TestRulesetPublisher::~TestRulesetPublisher() = default;

void TestRulesetPublisher::SetRuleset(const TestRuleset& unindexed_ruleset) {
  const std::string& test_ruleset_content_version(base::NumberToString(
      base::Hash(std::string(unindexed_ruleset.contents.begin(),
                             unindexed_ruleset.contents.end()))));
  subresource_filter::UnindexedRulesetInfo unindexed_ruleset_info;
  unindexed_ruleset_info.content_version = test_ruleset_content_version;
  unindexed_ruleset_info.ruleset_path = unindexed_ruleset.path;
  RulesetDistributionListener listener(ruleset_service_);
  ruleset_service_->IndexAndStoreAndPublishRulesetIfNeeded(
      unindexed_ruleset_info);
  listener.AwaitDistribution();
}

}  // namespace testing
}  // namespace subresource_filter
