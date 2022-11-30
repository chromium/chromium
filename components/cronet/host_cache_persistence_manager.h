// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_HOST_CACHE_PERSISTENCE_MANAGER_H_
#define COMPONENTS_CRONET_HOST_CACHE_PERSISTENCE_MANAGER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/prefs/pref_change_registrar.h"
#include "net/dns/host_cache.h"
#include "net/log/net_log_with_source.h"

class PrefService;

namespace net {
class NetLog;
}

namespace cronet {
// Handles the interaction between HostCache and prefs for persistence.
// When notified of a change in the HostCache, starts a timer, or ignores if the
// timer is already running. When that timer expires, writes the current state
// of the HostCache to prefs.
//
// Can be used with synchronous or asynchronous prefs loading. Not appropriate
// for use outside of Cronet because its network and prefs operations run on
// the same sequence. Must be created after and destroyed before the HostCache
// and PrefService.
class HostCachePersistenceManager : public net::HostCache::PersistenceDelegate {
 public:
  // |cache| is the HostCache whose contents will be persisted. It must be
  // non-null and must outlive the HostCachePersistenceManager.
  // |pref_service| is the PrefService that will be used to persist the cache
  // contents. It must outlive the HostCachePersistenceManager.
  // |pref_name| is the name of the pref to read and write.
  // |delay| is the maximum time between a change in the cache and writing that
  // change to prefs.
  HostCachePersistenceManager(net::HostCache* cache,
                              PrefService* pref_service,
                              std::string pref_name,
                              base::TimeDelta delay,
                              net::NetLog* net_log);

  HostCachePersistenceManager(const HostCachePersistenceManager&) = delete;
  HostCachePersistenceManager& operator=(const HostCachePersistenceManager&) =
      delete;

  virtual ~HostCachePersistenceManager();

  // net::HostCache::PersistenceDelegate implementation
  void ScheduleWrite() override;

 private:
  // Gets the serialized HostCache and writes it to prefs.
  void WriteToDisk();
  // On initial prefs read, passes the serialized entries to the HostCache.
  void ReadFromDisk();

  const raw_ptr<net::HostCache> cache_;

  PrefChangeRegistrar registrar_;
  const raw_ptr<PrefService> pref_service_;
  const std::string pref_name_;
  bool writing_pref_;

  const base::TimeDelta delay_;
  base::OneShotTimer timer_;

  const net::NetLogWithSource net_log_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<HostCachePersistenceManager> weak_factory_{this};
};

}  // namespace cronet

#endif  // COMPONENTS_CRONET_HOST_CACHE_PERSISTENCE_MANAGER_H_
