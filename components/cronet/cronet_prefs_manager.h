// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_CRONET_PREFS_MANAGER_H_
#define COMPONENTS_CRONET_CRONET_PREFS_MANAGER_H_

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"

class JsonPrefStore;
class NetLog;
class PrefService;

namespace base {
class SingleThreadTaskRunner;
class SequencedTaskRunner;
}  // namespace base

namespace net {
class HostCache;
class NetLog;
class NetworkQualitiesPrefsManager;
class NetworkQualityEstimator;
class URLRequestContextBuilder;
}  // namespace net

namespace cronet {
class HostCachePersistenceManager;

// Manages the PrefService, JsonPrefStore and all associated persistence
// managers used by Cronet such as NetworkQualityPrefsManager,
// HostCachePersistenceManager, etc. The constructor, destructor and all
// other methods of this class should be called on the network thread.
class CronetPrefsManager {
 public:
  CronetPrefsManager(
      const std::string& storage_path,
      scoped_refptr<base::SingleThreadTaskRunner> network_task_runner,
      scoped_refptr<base::SequencedTaskRunner> file_task_runner,
      bool enable_network_quality_estimator,
      bool enable_host_cache_persistence,
      net::NetLog* net_log,
      net::URLRequestContextBuilder* context_builder);

  virtual ~CronetPrefsManager();

  void SetupNqePersistence(net::NetworkQualityEstimator* nqe);

  void SetupHostCachePersistence(net::HostCache* host_cache,
                                 int host_cache_persistence_delay_ms,
                                 net::NetLog* net_log);

  // Prepares |this| for shutdown.
  void PrepareForShutdown();

 private:
  // |pref_service_| should outlive the HttpServerPropertiesManager owned by
  // |host_cache_persistence_manager_|.
  std::unique_ptr<PrefService> pref_service_;
  scoped_refptr<JsonPrefStore> json_pref_store_;

  // Manages the writing and reading of the network quality prefs.
  std::unique_ptr<net::NetworkQualitiesPrefsManager>
      network_qualities_prefs_manager_;

  // Manages reading and writing the HostCache pref when persistence is enabled.
  // Must be destroyed before |context_| owned by CronetUrlContextAdapter
  // (because it owns the HostResolverImpl,
  // which owns the HostCache) and |pref_service_|.
  std::unique_ptr<HostCachePersistenceManager> host_cache_persistence_manager_;

  // Checks that all methods are called on the network thread.
  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(CronetPrefsManager);

};  // class CronetPrefsManager

}  // namespace cronet

#endif  // COMPONENTS_CRONET_CRONET_PREFS_MANAGER_H_
