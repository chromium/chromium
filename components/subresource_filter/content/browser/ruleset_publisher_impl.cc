// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/ruleset_publisher_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "components/subresource_filter/content/browser/ruleset_service.h"
#include "components/subresource_filter/core/browser/subresource_filter_constants.h"
#include "components/subresource_filter/core/common/common_features.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "ipc/ipc_channel_proxy.h"

namespace subresource_filter {

RulesetPublisherImpl::RulesetPublisherImpl(
    RulesetService* ruleset_service,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner)
    : ruleset_service_(ruleset_service),
      ruleset_dealer_(std::make_unique<VerifiedRulesetDealer::Handle>(
          std::move(blocking_task_runner))) {
  best_effort_task_runner_ =
      content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT});
  DCHECK(best_effort_task_runner_->BelongsToCurrentThread());
}

RulesetPublisherImpl::~RulesetPublisherImpl() = default;

void RulesetPublisherImpl::SetRulesetPublishedCallbackForTesting(
    base::OnceClosure callback) {
  ruleset_published_callback_ = std::move(callback);
}

void RulesetPublisherImpl::TryOpenAndSetRulesetFile(
    const base::FilePath& file_path,
    int expected_checksum,
    base::OnceCallback<void(RulesetFilePtr)> callback) {
  GetRulesetDealer()->TryOpenAndSetRulesetFile(file_path, expected_checksum,
                                               std::move(callback));
}

void RulesetPublisherImpl::PublishNewRulesetVersion(
    RulesetFilePtr ruleset_data) {
  DCHECK(ruleset_data);
  DCHECK(ruleset_data->IsValid());
  ruleset_data_.reset();

  // If Ad Tagging is running, then every request does a lookup and it's
  // important that we verify the ruleset early on.
  if (base::FeatureList::IsEnabled(kAdTagging)) {
    // Even though the handle will immediately be destroyed, it will still
    // validate the ruleset on its task runner.
    VerifiedRuleset::Handle ruleset_handle(GetRulesetDealer());
  }

  ruleset_data_ = std::move(ruleset_data);
  for (auto it = content::RenderProcessHost::AllHostsIterator(); !it.IsAtEnd();
       it.Advance()) {
    SendRulesetToRenderProcess(ruleset_data_.get(), it.GetCurrentValue());
  }

  if (!ruleset_published_callback_.is_null())
    std::move(ruleset_published_callback_).Run();
}

scoped_refptr<base::SingleThreadTaskRunner>
RulesetPublisherImpl::BestEffortTaskRunner() {
  return best_effort_task_runner_;
}

VerifiedRulesetDealer::Handle* RulesetPublisherImpl::GetRulesetDealer() {
  return ruleset_dealer_.get();
}

void RulesetPublisherImpl::IndexAndStoreAndPublishRulesetIfNeeded(
    const UnindexedRulesetInfo& unindexed_ruleset_info) {
  DCHECK(ruleset_service_);
  ruleset_service_->IndexAndStoreAndPublishRulesetIfNeeded(
      unindexed_ruleset_info);
}

void RulesetPublisherImpl::OnRenderProcessHostCreated(
    content::RenderProcessHost* rph) {
  if (!ruleset_data_ || !ruleset_data_->IsValid())
    return;
  SendRulesetToRenderProcess(ruleset_data_.get(), rph);
}

void RulesetPublisherImpl::SendRulesetToRenderProcess(
    base::File* file,
    content::RenderProcessHost* rph) {
  DCHECK(rph);
  DCHECK(file);
  DCHECK(file->IsValid());
  if (!rph->GetChannel())
    return;
  mojo::AssociatedRemote<mojom::SubresourceFilterRulesetObserver>
      subresource_filter;
  rph->GetChannel()->GetRemoteAssociatedInterface(&subresource_filter);
  subresource_filter->SetRulesetForProcess(file->Duplicate());
}

}  // namespace subresource_filter
