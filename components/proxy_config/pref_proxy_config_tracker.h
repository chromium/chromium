// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXY_CONFIG_PREF_PROXY_CONFIG_TRACKER_H_
#define COMPONENTS_PROXY_CONFIG_PREF_PROXY_CONFIG_TRACKER_H_

#include <memory>

#include "components/proxy_config/proxy_config_export.h"

namespace net {
class ProxyConfigService;
}

// Interface for a class that tracks proxy preferences. The purpose of the
// concrete class is to track changes in the Preferences, to translates the
// preferences to net::ProxyConfig and to push the result over to a
// net::ProxyConfigService onto the IO thread.
class PROXY_CONFIG_EXPORT PrefProxyConfigTracker {
 public:
  PrefProxyConfigTracker();

  PrefProxyConfigTracker(const PrefProxyConfigTracker&) = delete;
  PrefProxyConfigTracker& operator=(const PrefProxyConfigTracker&) = delete;

  virtual ~PrefProxyConfigTracker();

  // Creates a net::ProxyConfigService and keeps a pointer to it. After this
  // call, this tracker forwards any changes of proxy preferences to the created
  // ProxyConfigService. The returned ProxyConfigService must not be deleted
  // before DetachFromPrefService was called. Takes ownership of the passed
  // |base_service|, which can be NULL. This |base_service| provides the proxy
  // settings of the OS (except of ChromeOS). This must be called on the
  // UI thread.  May only be called once on a PrefProxyConfigTracker.
  virtual std::unique_ptr<net::ProxyConfigService>
  CreateTrackingProxyConfigService(
      std::unique_ptr<net::ProxyConfigService> base_service) = 0;

  // Releases the PrefService passed upon construction and the |base_service|
  // passed to CreateTrackingProxyConfigService. This must be called on the UI
  // thread.
  virtual void DetachFromPrefService() = 0;
};

#endif  // COMPONENTS_PROXY_CONFIG_PREF_PROXY_CONFIG_TRACKER_H_
