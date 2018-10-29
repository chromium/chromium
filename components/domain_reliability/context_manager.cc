// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/context_manager.h"

#include <utility>

#include "base/metrics/histogram_macros.h"

namespace domain_reliability {

DomainReliabilityContextManager::DomainReliabilityContextManager(
    DomainReliabilityContext::Factory* context_factory)
    : context_factory_(context_factory) {
}

DomainReliabilityContextManager::~DomainReliabilityContextManager() {
  RemoveContexts(
      base::Callback<bool(const GURL&)>() /* no filter - delete everything */);
}

void DomainReliabilityContextManager::RouteBeacon(
    std::unique_ptr<DomainReliabilityBeacon> beacon) {
  DomainReliabilityContext* context = GetContextForHost(beacon->url.host());
  if (!context)
    return;

  context->OnBeacon(std::move(beacon));
}

void DomainReliabilityContextManager::SetConfig(
    const GURL& origin,
    std::unique_ptr<DomainReliabilityConfig> config,
    base::TimeDelta max_age) {
  std::string key = origin.host();

  if (!contexts_.count(key) && !removed_contexts_.count(key)) {
    LOG(WARNING) << "Ignoring NEL header for unknown origin " << origin.spec()
                 << ".";
    return;
  }

  if (contexts_.count(key)) {
    // Currently, there is no easy way to change the config of a context, so
    // updating the config requires recreating the context, which loses
    // pending beacons and collector backoff state. Therefore, don't do so
    // needlessly; make sure the config has actually changed before recreating
    // the context.
    bool config_same = contexts_[key]->config().Equals(*config);
    if (!config_same) {
      DVLOG(1) << "Ignoring unchanged NEL header for existing origin "
               << origin.spec() << ".";
      return;
    }
    // TODO(juliatuttle): Make Context accept Config changes.
  }

  DVLOG(1) << "Adding/replacing context for existing origin " << origin.spec()
           << ".";
  removed_contexts_.erase(key);
  config->origin = origin;
  AddContextForConfig(std::move(config));
}

void DomainReliabilityContextManager::ClearConfig(const GURL& origin) {
  std::string key = origin.host();

  if (contexts_.count(key)) {
    DVLOG(1) << "Removing context for existing origin " << origin.spec() << ".";
    contexts_.erase(key);
    removed_contexts_.insert(key);
  }
}

void DomainReliabilityContextManager::ClearBeacons(
    const base::Callback<bool(const GURL&)>& origin_filter) {
  for (auto& context_entry : contexts_) {
    if (origin_filter.is_null() ||
        origin_filter.Run(context_entry.second->config().origin)) {
      context_entry.second->ClearBeacons();
    }
  }
}

DomainReliabilityContext* DomainReliabilityContextManager::AddContextForConfig(
    std::unique_ptr<const DomainReliabilityConfig> config) {
  std::string key = config->origin.host();
  // TODO(juliatuttle): Convert this to actual origin.

  std::unique_ptr<DomainReliabilityContext> context =
      context_factory_->CreateContextForConfig(std::move(config));
  DomainReliabilityContext** entry = &contexts_[key];
  if (*entry)
    delete *entry;

  *entry = context.release();
  return *entry;
}

void DomainReliabilityContextManager::RemoveContexts(
    const base::Callback<bool(const GURL&)>& origin_filter) {
  for (auto it = contexts_.begin(); it != contexts_.end();) {
    if (!origin_filter.is_null() &&
        !origin_filter.Run(it->second->config().origin)) {
      ++it;
      continue;
    }

    delete it->second;
    it = contexts_.erase(it);
  }
}

std::unique_ptr<base::Value> DomainReliabilityContextManager::GetWebUIData()
    const {
  std::unique_ptr<base::ListValue> contexts_value(new base::ListValue());
  for (const auto& context_entry : contexts_)
    contexts_value->Append(context_entry.second->GetWebUIData());
  return std::move(contexts_value);
}

DomainReliabilityContext* DomainReliabilityContextManager::GetContextForHost(
    const std::string& host) {
  ContextMap::const_iterator context_it;

  context_it = contexts_.find(host);
  if (context_it != contexts_.end())
    return context_it->second;

  size_t dot_pos = host.find('.');
  if (dot_pos == std::string::npos)
    return nullptr;

  // TODO(juliatuttle): Make sure parent is not in PSL before using.

  std::string parent_host = host.substr(dot_pos + 1);
  context_it = contexts_.find(parent_host);
  if (context_it != contexts_.end()
      && context_it->second->config().include_subdomains) {
    return context_it->second;
  }

  return nullptr;
}

}  // namespace domain_reliability
