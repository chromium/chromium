// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_RULESET_PUBLISHER_IMPL_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_RULESET_PUBLISHER_IMPL_H_

#include <memory>

#include "base/callback.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "components/subresource_filter/content/browser/ruleset_publisher.h"
#include "components/subresource_filter/content/browser/ruleset_version.h"
#include "components/subresource_filter/content/browser/verified_ruleset_dealer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace subresource_filter {

class RulesetService;

// The implementation of RulesetPublisher. Owned by the underlying
// RulesetService. Its main responsibility is receiving new versions of
// subresource filtering rules from the RulesetService, and distributing them to
// renderer processes, where they will be memory-mapped as-needed by the
// UnverifiedRulesetDealer.
class RulesetPublisherImpl : public RulesetPublisher,
                             public content::NotificationObserver {
 public:
  RulesetPublisherImpl(
      RulesetService* ruleset_service,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner);
  ~RulesetPublisherImpl() override;

  // RulesetPublisher:
  void TryOpenAndSetRulesetFile(
      const base::FilePath& file_path,
      int expected_checksum,
      base::OnceCallback<void(base::File)> callback) override;
  void PublishNewRulesetVersion(base::File ruleset_data) override;
  scoped_refptr<base::SingleThreadTaskRunner> BestEffortTaskRunner() override;
  VerifiedRulesetDealer::Handle* GetRulesetDealer() override;
  void SetRulesetPublishedCallbackForTesting(
      base::OnceClosure callback) override;

  // Forwards calls to the underlying ruleset_service_.
  void IndexAndStoreAndPublishRulesetIfNeeded(
      const UnindexedRulesetInfo& unindex_ruleset_info);

 private:
  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  content::NotificationRegistrar notification_registrar_;
  base::File ruleset_data_;
  base::OnceClosure ruleset_published_callback_;

  // The service owns the publisher, and therefore outlives it.
  RulesetService* ruleset_service_;
  std::unique_ptr<VerifiedRulesetDealer::Handle> ruleset_dealer_;
  scoped_refptr<base::SingleThreadTaskRunner> best_effort_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(RulesetPublisherImpl);
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_RULESET_SERVICE_H_
