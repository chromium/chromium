// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_rewrite/browser/url_request_rewrite_rules_manager.h"

#include "components/url_rewrite/browser/url_request_rewrite_rules_validation.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace url_rewrite {

UrlRequestRewriteRulesManager::UrlRequestRewriteRulesManager() = default;

UrlRequestRewriteRulesManager::~UrlRequestRewriteRulesManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool UrlRequestRewriteRulesManager::AddWebContents(
    content::WebContents* web_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(web_contents);

  if (updaters_.count(web_contents) > 0) {
    return false;
  }

  auto updater = std::make_unique<Updater>(web_contents, cached_rules_);
  updaters_.emplace(web_contents, std::move(updater));
  return true;
}

bool UrlRequestRewriteRulesManager::RemoveWebContents(
    content::WebContents* web_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(web_contents);

  auto iter = updaters_.find(web_contents);
  if (iter == updaters_.end()) {
    return false;
  }

  updaters_.erase(iter);
  return true;
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
  for (const auto& updater_pair : updaters_) {
    updater_pair.second->OnRulesUpdated(cached_rules_);
  }

  return true;
}

const scoped_refptr<UrlRequestRewriteRules>&
UrlRequestRewriteRulesManager::GetCachedRules() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return cached_rules_;
}

size_t UrlRequestRewriteRulesManager::GetUpdatersSizeForTesting() const {
  return updaters_.size();
}

UrlRequestRewriteRulesManager::Updater::Updater(
    content::WebContents* web_contents,
    const scoped_refptr<UrlRequestRewriteRules>& cached_rules)
    : content::WebContentsObserver(web_contents), cached_rules_(cached_rules) {
  web_contents->ForEachRenderFrameHost(
      [this](content::RenderFrameHost* render_frame_host) {
        MaybeRegisterExistingRenderFrame(render_frame_host);
      });
}

UrlRequestRewriteRulesManager::Updater::~Updater() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void UrlRequestRewriteRulesManager::Updater::OnRulesUpdated(
    const scoped_refptr<UrlRequestRewriteRules>& cached_rules) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(cached_rules);

  cached_rules_ = cached_rules;

  // Send the updated rules to the updaters.
  for (const auto& receiver_pair : active_remotes_) {
    receiver_pair.second->OnRulesUpdated(mojo::Clone(cached_rules_->data));
  }
}

void UrlRequestRewriteRulesManager::Updater::MaybeRegisterExistingRenderFrame(
    content::RenderFrameHost* render_frame_host) {
  if (render_frame_host->IsRenderFrameLive()) {
    // Call RenderFrameCreated() for frames that were created before this
    // observer started observing this WebContents.
    RenderFrameCreated(render_frame_host);
  }
}

void UrlRequestRewriteRulesManager::Updater::RenderFrameCreated(
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

void UrlRequestRewriteRulesManager::Updater::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  size_t removed = active_remotes_.erase(render_frame_host->GetGlobalId());
  DCHECK_EQ(removed, 1u);
}

}  // namespace url_rewrite
