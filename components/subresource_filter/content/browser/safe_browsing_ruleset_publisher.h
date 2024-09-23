// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SAFE_BROWSING_RULESET_PUBLISHER_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SAFE_BROWSING_RULESET_PUBLISHER_H_

#include <memory>
#include <utility>

#include "base/files/file.h"
#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"
#include "components/subresource_filter/content/shared/browser/ruleset_publisher.h"
#include "components/subresource_filter/content/shared/browser/ruleset_service.h"

namespace content {
class RenderProcessHost;
}  // namespace content

namespace subresource_filter {

class RulesetService;

class SafeBrowsingRulesetPublisher : public RulesetPublisher {
 public:
  SafeBrowsingRulesetPublisher(
      RulesetService* ruleset_service,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner)
      : RulesetPublisher(ruleset_service,
                         std::move(blocking_task_runner),
                         ruleset_service->config()) {}

  SafeBrowsingRulesetPublisher(const SafeBrowsingRulesetPublisher&) = delete;
  SafeBrowsingRulesetPublisher& operator=(const SafeBrowsingRulesetPublisher&) =
      delete;

  class Factory : public RulesetPublisher::Factory {
   public:
    std::unique_ptr<RulesetPublisher> Create(
        RulesetService* ruleset_service,
        scoped_refptr<base::SequencedTaskRunner> blocking_task_runner)
        const override;
  };

  ~SafeBrowsingRulesetPublisher() override = default;

  void SendRulesetToRenderProcess(base::File* file,
                                  content::RenderProcessHost* rph) override;
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SAFE_BROWSING_RULESET_PUBLISHER_H_
