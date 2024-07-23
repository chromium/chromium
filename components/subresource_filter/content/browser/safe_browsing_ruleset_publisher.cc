// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/safe_browsing_ruleset_publisher.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/files/file.h"
#include "base/memory/ref_counted.h"
#include "components/subresource_filter/content/browser/safe_browsing_ruleset_publisher.h"
#include "components/subresource_filter/content/shared/browser/ruleset_service.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "content/public/browser/render_process_host.h"
#include "ipc/ipc_channel_proxy.h"
#include "mojo/public/cpp/bindings/associated_remote.h"

namespace subresource_filter {

std::unique_ptr<RulesetPublisher> SafeBrowsingRulesetPublisher::Factory::Create(
    RulesetService* ruleset_service,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner) const {
  return std::make_unique<SafeBrowsingRulesetPublisher>(
      ruleset_service, std::move(blocking_task_runner));
}

void SafeBrowsingRulesetPublisher::SendRulesetToRenderProcess(
    base::File* file,
    content::RenderProcessHost* rph) {
  CHECK(rph, base::NotFatalUntil::M129);
  CHECK(file, base::NotFatalUntil::M129);
  CHECK(file->IsValid(), base::NotFatalUntil::M129);
  if (!rph->GetChannel()) {
    return;
  }
  mojo::AssociatedRemote<mojom::SubresourceFilterRulesetObserver>
      subresource_filter;
  rph->GetChannel()->GetRemoteAssociatedInterface(&subresource_filter);
  subresource_filter->SetRulesetForProcess(file->Duplicate());
}

}  // namespace subresource_filter
