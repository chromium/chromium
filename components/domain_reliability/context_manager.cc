// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/context_manager.h"

#include <utility>

#include "base/functional/callback_helpers.h"
#include "components/domain_reliability/google_configs.h"
#include "net/base/url_util.h"

namespace domain_reliability {

DomainReliabilityContextManager::DomainReliabilityContextManager(
    const MockableTime* time,
    const std::string& upload_reporter_string,
    DomainReliabilityContext::UploadAllowedCallback upload_allowed_callback,
    DomainReliabilityDispatcher* dispatcher)
    : time_(time),
      upload_reporter_string_(upload_reporter_string),
      upload_allowed_callback_(upload_allowed_callback),
      dispatcher_(dispatcher) {
  DCHECK(time_);
  DCHECK(dispatcher_);
}

DomainReliabilityContextManager::~DomainReliabilityContextManager() = default;

void DomainReliabilityContextManager::RouteBeacon(
    std::unique_ptr<DomainReliabilityBeacon> beacon) {
  const std::string& beacon_host = beacon->url.host();

  // An exact match for the host always takes priority.
  DomainReliabilityContext* context_to_use = GetContext(beacon_host);
  if (context_to_use) {
    context_to_use->OnBeacon(std::move(beacon));
    return;
  }

  DomainReliabilityContext* superdomain_context =
      GetSuperdomainContext(beacon_host);

  // Try to get a Google config which may match the host itself, or the host's
  // parent domain.
  std::unique_ptr<const DomainReliabilityConfig> google_config =
      MaybeGetGoogleConfig(beacon_host);

  if (!google_config) {
    if (superdomain_context)
      superdomain_context->OnBeacon(std::move(beacon));
    return;
  }

  context_to_use = superdomain_context;
  bool google_config_is_exact = (google_config->origin.host() == beacon_host);

  // An exact match takes priority over an existing superdomain context, if any
  // exists.
  if (google_config_is_exact || !context_to_use)
    context_to_use = AddContextForConfig(std::move(google_config));

  context_to_use->OnBeacon(std::move(beacon));
}

void DomainReliabilityContextManager::ClearBeacons(
    const base::RepeatingCallback<bool(const url::Origin&)>& origin_filter) {
  for (auto& context_entry : contexts_) {
    if (origin_filter.is_null() ||
        origin_filter.Run(context_entry.second->config().origin)) {
      context_entry.second->ClearBeacons();
    }
  }
}

DomainReliabilityContext* DomainReliabilityContextManager::AddContextForConfig(
    std::unique_ptr<const DomainReliabilityConfig> config) {
  const std::string& key = config->origin.host();
  auto pair = contexts_.insert(
      std::make_pair(key, CreateContextForConfig(std::move(config))));

  // Insertion should have succeeded (the key should not have already existed).
  DCHECK(pair.second);
  return pair.first->second.get();
}

void DomainReliabilityContextManager::RemoveContexts(
    const base::RepeatingCallback<bool(const url::Origin&)>& origin_filter) {
  if (origin_filter.is_null()) {
    contexts_.clear();
    return;
  }

  for (auto it = contexts_.begin(); it != contexts_.end();) {
    if (origin_filter.Run(it->second->config().origin)) {
      it = contexts_.erase(it);
      continue;
    }
    ++it;
  }
}

DomainReliabilityContext* DomainReliabilityContextManager::GetContext(
    const std::string& host) const {
  ContextMap::const_iterator context_it = contexts_.find(host);
  if (context_it == contexts_.end())
    return nullptr;
  return context_it->second.get();
}

DomainReliabilityContext*
DomainReliabilityContextManager::GetSuperdomainContext(
    const std::string& host) const {
  // TODO(juliatuttle): Make sure parent is not in PSL before using.
  std::string parent_host = net::GetSuperdomain(host);
  if (parent_host.empty())
    return nullptr;

  DomainReliabilityContext* context = GetContext(parent_host);
  if (context && context->config().include_subdomains)
    return context;

  return nullptr;
}

void DomainReliabilityContextManager::OnNetworkChanged(base::TimeTicks now) {
  last_network_change_time_ = now;
}

void DomainReliabilityContextManager::SetUploader(
    DomainReliabilityUploader* uploader) {
  DCHECK(!uploader_);
  DCHECK(uploader);
  uploader_ = uploader;
}

std::unique_ptr<DomainReliabilityContext>
DomainReliabilityContextManager::CreateContextForConfig(
    std::unique_ptr<const DomainReliabilityConfig> config) const {
  DCHECK(config);
  DCHECK(config->IsValid());
  DCHECK(uploader_);

  return std::make_unique<DomainReliabilityContext>(
      time_, DomainReliabilityScheduler::Params::GetFromFieldTrialsOrDefaults(),
      upload_reporter_string_, &last_network_change_time_,
      upload_allowed_callback_, dispatcher_, uploader_, std::move(config));
}

}  // namespace domain_reliability
