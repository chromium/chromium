// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_BROWSER_FINGERPRINTING_PROTECTION_RULESET_PUBLISHER_H_
#define COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_BROWSER_FINGERPRINTING_PROTECTION_RULESET_PUBLISHER_H_

#include <memory>
#include <utility>

#include "base/files/file.h"
#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_constants.h"
#include "components/subresource_filter/content/shared/browser/ruleset_publisher.h"

namespace content {
class RenderProcessHost;
}  // namespace content

namespace subresource_filter {
class RulesetService;
}  // namespace subresource_filter

namespace fingerprinting_protection_filter {

class FingerprintingProtectionRulesetPublisher
    : public subresource_filter::RulesetPublisher {
 public:
  FingerprintingProtectionRulesetPublisher(
      subresource_filter::RulesetService* ruleset_service,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner)
      : subresource_filter::RulesetPublisher(
            ruleset_service,
            std::move(blocking_task_runner),
            kFingerprintingProtectionRulesetConfig) {}

  FingerprintingProtectionRulesetPublisher(
      const FingerprintingProtectionRulesetPublisher&) = delete;
  FingerprintingProtectionRulesetPublisher& operator=(
      const FingerprintingProtectionRulesetPublisher&) = delete;

  class Factory : public subresource_filter::RulesetPublisher::Factory {
   public:
    std::unique_ptr<subresource_filter::RulesetPublisher> Create(
        subresource_filter::RulesetService* ruleset_service,
        scoped_refptr<base::SequencedTaskRunner> blocking_task_runner)
        const override;
  };

  ~FingerprintingProtectionRulesetPublisher() override = default;

  void SendRulesetToRenderProcess(base::File* file,
                                  content::RenderProcessHost* rph) override;
};

}  // namespace fingerprinting_protection_filter

#endif  // COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_BROWSER_FINGERPRINTING_PROTECTION_RULESET_PUBLISHER_H_
