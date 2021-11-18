// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_rewrite/browser/url_request_rewrite_rules_manager.h"

#include "components/url_rewrite/browser/url_request_rewrite_rules_validation.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace url_rewrite {

// static
std::unique_ptr<UrlRequestRewriteRulesManager>
UrlRequestRewriteRulesManager::CreateForTesting() {
  return base::WrapUnique(new UrlRequestRewriteRulesManager());
}

UrlRequestRewriteRulesManager::UrlRequestRewriteRulesManager(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

UrlRequestRewriteRulesManager::~UrlRequestRewriteRulesManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool UrlRequestRewriteRulesManager::OnRulesUpdated(
    mojom::UrlRequestRewriteRulesPtr rules) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!ValidateRules(rules.get())) {
    return false;
  }

  cached_rules_ =
      base::MakeRefCounted<UrlRequestRewriteRules>(std::move(rules));
  // Send the updated rules to the receivers.
  for (const auto& receiver_pair : active_remotes_) {
    receiver_pair.second->OnRulesUpdated(mojo::Clone(cached_rules_->data));
  }

  return true;
}

scoped_refptr<UrlRequestRewriteRules>&
UrlRequestRewriteRulesManager::GetCachedRules() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return cached_rules_;
}

UrlRequestRewriteRulesManager::UrlRequestRewriteRulesManager() = default;

void UrlRequestRewriteRulesManager::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Register the frame rules receiver.
  mojo::AssociatedRemote<mojom::UrlRequestRulesReceiver> rules_receiver;
  render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(
      &rules_receiver);
  auto iter = active_remotes_.emplace(render_frame_host->GetGlobalId(),
                                      std::move(rules_receiver));
  DCHECK(iter.second);

  if (cached_rules_) {
    // Send an initial set of rules.
    iter.first->second->OnRulesUpdated(mojo::Clone(cached_rules_->data));
  }
}

void UrlRequestRewriteRulesManager::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  size_t removed = active_remotes_.erase(render_frame_host->GetGlobalId());
  DCHECK_EQ(removed, 1u);
}

}  // namespace url_rewrite
