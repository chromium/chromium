// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proxy_config/ios/proxy_service_factory.h"

#include <utility>

#include <memory>

#include "base/task/post_task.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"
#include "net/proxy_resolution/proxy_config_service.h"
#include "net/proxy_resolution/proxy_resolution_service.h"

// static
std::unique_ptr<net::ProxyConfigService>
ProxyServiceFactory::CreateProxyConfigService(PrefProxyConfigTracker* tracker) {
  std::unique_ptr<net::ProxyConfigService> base_service(
      net::ProxyResolutionService::CreateSystemProxyConfigService(
          base::CreateSingleThreadTaskRunner({web::WebThread::IO})));
  return tracker->CreateTrackingProxyConfigService(std::move(base_service));
}

// static
std::unique_ptr<PrefProxyConfigTracker>
ProxyServiceFactory::CreatePrefProxyConfigTrackerOfProfile(
    PrefService* browser_state_prefs,
    PrefService* local_state_prefs) {
  return std::make_unique<PrefProxyConfigTrackerImpl>(
      browser_state_prefs,
      base::CreateSingleThreadTaskRunner({web::WebThread::IO}));
}

// static
std::unique_ptr<PrefProxyConfigTracker>
ProxyServiceFactory::CreatePrefProxyConfigTrackerOfLocalState(
    PrefService* local_state_prefs) {
  return std::make_unique<PrefProxyConfigTrackerImpl>(
      local_state_prefs,
      base::CreateSingleThreadTaskRunner({web::WebThread::IO}));
}

// static
std::unique_ptr<net::ProxyResolutionService>
ProxyServiceFactory::CreateProxyResolutionService(
    net::NetLog* net_log,
    net::URLRequestContext* context,
    net::NetworkDelegate* network_delegate,
    std::unique_ptr<net::ProxyConfigService> proxy_config_service,
    bool quick_check_enabled) {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  std::unique_ptr<net::ProxyResolutionService> proxy_resolution_service(
      net::ProxyResolutionService::CreateUsingSystemProxyResolver(
          std::move(proxy_config_service), net_log));
  proxy_resolution_service->set_quick_check_enabled(quick_check_enabled);
  return proxy_resolution_service;
}
