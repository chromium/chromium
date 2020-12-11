// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOMAIN_RELIABILITY_CONTEXT_MANAGER_H_
#define COMPONENTS_DOMAIN_RELIABILITY_CONTEXT_MANAGER_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/domain_reliability/beacon.h"
#include "components/domain_reliability/config.h"
#include "components/domain_reliability/context.h"
#include "components/domain_reliability/domain_reliability_export.h"
#include "url/gurl.h"

namespace domain_reliability {

// Owns DomainReliabilityContexts, receives Beacons and queues them in the
// appropriate Context.
// TODO(chlily): This class should be merged with the DomainReliabilityMonitor.
class DOMAIN_RELIABILITY_EXPORT DomainReliabilityContextManager {
 public:
  DomainReliabilityContextManager(
      const MockableTime* time,
      const std::string& upload_reporter_string,
      DomainReliabilityContext::UploadAllowedCallback upload_allowed_callback,
      DomainReliabilityDispatcher* dispatcher);
  ~DomainReliabilityContextManager();

  // If |url| maps to a context added to this manager, calls |OnBeacon| on
  // that context with |beacon|. Otherwise, does nothing.
  // Prefers contexts that are an exact match for the beacon's url's hostname
  // over a subdomain-inclusive context for the beacon's superdomain.
  // If the beacon should be routed to a Google config that has not yet been
  // created, this will create it first.
  void RouteBeacon(std::unique_ptr<DomainReliabilityBeacon> beacon);

  // Calls |ClearBeacons| on all contexts matched by |origin_filter| added
  // to this manager, but leaves the contexts themselves intact. A null
  // |origin_filter| is interpreted as an always-true filter, indicating
  // complete deletion.
  void ClearBeacons(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter);

  // Creates and stores a context for the given |config|, which should be for an
  // origin distinct from any existing ones. Returns pointer to the inserted
  // context.
  DomainReliabilityContext* AddContextForConfig(
      std::unique_ptr<const DomainReliabilityConfig> config);

  // Removes all contexts matched by |origin_filter| from this manager
  // (discarding all queued beacons in the process). A null |origin_filter|
  // is interpreted as an always-true filter, indicating complete deletion.
  void RemoveContexts(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter);

  // Finds a context for the exact domain |host|. Otherwise returns nullptr.
  DomainReliabilityContext* GetContext(const std::string& host) const;

  // Finds a context for the parent domain of |host|, which must include
  // subdomains. Otherwise returns nullptr.
  DomainReliabilityContext* GetSuperdomainContext(
      const std::string& host) const;

  void OnNetworkChanged(base::TimeTicks now);

  // Called by the Monitor during initialization. Should be called exactly once.
  // |uploader_| needs to be set before any contexts are created.
  void SetUploader(DomainReliabilityUploader* uploader);

  std::unique_ptr<base::Value> GetWebUIData() const;

  size_t contexts_size_for_testing() const { return contexts_.size(); }

 private:
  // Maps the hostname to the DomainReliabilityContext for that origin.
  using ContextMap =
      std::map<std::string, std::unique_ptr<DomainReliabilityContext>>;

  std::unique_ptr<DomainReliabilityContext> CreateContextForConfig(
      std::unique_ptr<const DomainReliabilityConfig> config) const;

  // |time_| is owned by the Monitor and a copy of this pointer is given to
  // every Context.
  const MockableTime* const time_;
  const std::string upload_reporter_string_;
  base::TimeTicks last_network_change_time_;
  DomainReliabilityContext::UploadAllowedCallback upload_allowed_callback_;
  // |dispatcher_| is owned by the Monitor and a copy of this pointer is given
  // to every Context.
  DomainReliabilityDispatcher* const dispatcher_;
  // |uploader_| is owned by the Monitor. Expected to be non-null after
  // initialization because it should be set by the Monitor, as long as the
  // Monitor has been fully initialized. A copy of this pointer is given to
  // every Context.
  DomainReliabilityUploader* uploader_ = nullptr;

  ContextMap contexts_;

  DISALLOW_COPY_AND_ASSIGN(DomainReliabilityContextManager);
};

}  // namespace domain_reliability

#endif  // COMPONENTS_DOMAIN_RELIABILITY_CONTEXT_MANAGER_H_
