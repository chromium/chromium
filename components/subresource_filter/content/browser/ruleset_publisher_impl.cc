// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/ruleset_publisher_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "components/subresource_filter/content/browser/ruleset_service.h"
#include "components/subresource_filter/content/common/subresource_filter_messages.h"
#include "components/subresource_filter/core/browser/subresource_filter_constants.h"
#include "components/subresource_filter/core/common/common_features.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "ipc/ipc_platform_file.h"

namespace subresource_filter {

namespace {

void SendRulesetToRenderProcess(base::File* file,
                                content::RenderProcessHost* rph) {
  DCHECK(rph);
  DCHECK(file);
  DCHECK(file->IsValid());
  rph->Send(new SubresourceFilterMsg_SetRulesetForProcess(
      IPC::TakePlatformFileForTransit(file->Duplicate())));
}

// The file handle is closed when the argument goes out of scope.
void CloseFile(base::File) {}

// Posts the |file| handle to the file thread so it can be closed.
void CloseFileOnFileThread(base::File* file) {
  if (!file->IsValid())
    return;
  base::PostTask(
      FROM_HERE,
      {base::ThreadPool(), base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(&CloseFile, std::move(*file)));
}

}  // namespace

RulesetPublisherImpl::RulesetPublisherImpl(
    RulesetService* ruleset_service,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner)
    : ruleset_service_(ruleset_service),
      ruleset_dealer_(std::make_unique<VerifiedRulesetDealer::Handle>(
          std::move(blocking_task_runner))) {
  best_effort_task_runner_ = base::CreateSingleThreadTaskRunner(
      {content::BrowserThread::UI, base::TaskPriority::BEST_EFFORT});
  DCHECK(best_effort_task_runner_->BelongsToCurrentThread());
  // Must rely on notifications as RenderProcessHostObserver::RenderProcessReady
  // would only be called after queued IPC messages (potentially triggering a
  // navigation) had already been sent to the new renderer.
  notification_registrar_.Add(
      this, content::NOTIFICATION_RENDERER_PROCESS_CREATED,
      content::NotificationService::AllBrowserContextsAndSources());
}

RulesetPublisherImpl::~RulesetPublisherImpl() {
  CloseFileOnFileThread(&ruleset_data_);
}

void RulesetPublisherImpl::SetRulesetPublishedCallbackForTesting(
    base::OnceClosure callback) {
  ruleset_published_callback_ = std::move(callback);
}

void RulesetPublisherImpl::TryOpenAndSetRulesetFile(
    const base::FilePath& file_path,
    int expected_checksum,
    base::OnceCallback<void(base::File)> callback) {
  GetRulesetDealer()->TryOpenAndSetRulesetFile(file_path, expected_checksum,
                                               std::move(callback));
}

void RulesetPublisherImpl::PublishNewRulesetVersion(base::File ruleset_data) {
  DCHECK(ruleset_data.IsValid());
  CloseFileOnFileThread(&ruleset_data_);

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
    SendRulesetToRenderProcess(&ruleset_data_, it.GetCurrentValue());
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

void RulesetPublisherImpl::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(type, content::NOTIFICATION_RENDERER_PROCESS_CREATED);
  if (!ruleset_data_.IsValid())
    return;
  SendRulesetToRenderProcess(
      &ruleset_data_,
      content::Source<content::RenderProcessHost>(source).ptr());
}

}  // namespace subresource_filter
