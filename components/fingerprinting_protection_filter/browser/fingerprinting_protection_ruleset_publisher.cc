// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_ruleset_publisher.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/files/file.h"
#include "base/memory/ref_counted.h"
#include "components/fingerprinting_protection_filter/mojom/fingerprinting_protection_filter.mojom.h"
#include "components/subresource_filter/content/shared/browser/ruleset_publisher.h"
#include "components/subresource_filter/content/shared/browser/ruleset_service.h"
#include "content/public/browser/render_process_host.h"
#include "ipc/ipc_channel_proxy.h"
#include "mojo/public/cpp/bindings/associated_remote.h"

namespace fingerprinting_protection_filter {

std::unique_ptr<subresource_filter::RulesetPublisher>
FingerprintingProtectionRulesetPublisher::Factory::Create(
    subresource_filter::RulesetService* ruleset_service,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner) const {
  return std::make_unique<FingerprintingProtectionRulesetPublisher>(
      ruleset_service, std::move(blocking_task_runner));
}

void FingerprintingProtectionRulesetPublisher::SendRulesetToRenderProcess(
    base::File* file,
    content::RenderProcessHost* rph) {
  CHECK(rph, base::NotFatalUntil::M129);
  CHECK(file, base::NotFatalUntil::M129);
  CHECK(file->IsValid(), base::NotFatalUntil::M129);
  if (!rph->GetChannel()) {
    return;
  }
  mojo::AssociatedRemote<mojom::FingerprintingProtectionRulesetObserver>
      ruleset_observer;
  rph->GetChannel()->GetRemoteAssociatedInterface(&ruleset_observer);
  ruleset_observer->SetRulesetForProcess(file->Duplicate());
}

}  // namespace fingerprinting_protection_filter
