// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_CRONET_URL_REQUEST_CONTEXT_H_
#define COMPONENTS_CRONET_CRONET_URL_REQUEST_CONTEXT_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "components/prefs/json_pref_store.h"
#include "net/nqe/effective_connection_type.h"
#include "net/nqe/effective_connection_type_observer.h"
#include "net/nqe/network_quality_estimator.h"
#include "net/nqe/network_quality_observation_source.h"
#include "net/nqe/rtt_throughput_estimates_observer.h"

class PrefService;

namespace base {
class SingleThreadTaskRunner;
class TimeTicks;
}  // namespace base

namespace net {
enum EffectiveConnectionType;
class NetLog;
class ProxyConfigService;
class URLRequestContext;
class URLRequestContextGetter;
class FileNetLogObserver;
}  // namespace net

namespace cronet {
class CronetPrefsManager;
class TestUtil;

struct URLRequestContextConfig;

// Wrapper around net::URLRequestContext.
class CronetURLRequestContext {
 public:
  // Callback implemented by CronetURLRequestContext() caller and owned by
  // CronetURLRequestContext::NetworkTasks.
  class Callback {
   public:
    virtual ~Callback() = default;

    // Invoked on network thread when initialized.
    virtual void OnInitNetworkThread() = 0;

    // Invoked on network thread immediately prior to destruction.
    virtual void OnDestroyNetworkThread() = 0;

    // net::NetworkQualityEstimator::EffectiveConnectionTypeObserver forwarder.
    virtual void OnEffectiveConnectionTypeChanged(
        net::EffectiveConnectionType effective_connection_type) = 0;

    // net::NetworkQualityEstimator::RTTAndThroughputEstimatesObserver
    // forwarder.
    virtual void OnRTTOrThroughputEstimatesComputed(
        int32_t http_rtt_ms,
        int32_t transport_rtt_ms,
        int32_t downstream_throughput_kbps) = 0;

    // net::NetworkQualityEstimator::RTTObserver forwarder.
    virtual void OnRTTObservation(
        int32_t rtt_ms,
        int32_t timestamp_ms,
        net::NetworkQualityObservationSource source) = 0;

    // net::NetworkQualityEstimator::RTTObserver forwarder.
    virtual void OnThroughputObservation(
        int32_t throughput_kbps,
        int32_t timestamp_ms,
        net::NetworkQualityObservationSource source) = 0;

    // Callback for StopNetLog() that signals that it is safe to access
    // the NetLog files.
    virtual void OnStopNetLogCompleted() = 0;
  };

  // Constructs CronetURLRequestContext using |context_config|. The |callback|
  // is owned by |this| and is deleted on network thread.
  // All |callback| methods are invoked on network thread.
  // If the network_task_runner is not assigned, a network thread would be
  // created for network tasks. Otherwise the tasks would be running on the
  // assigned task runner.
  CronetURLRequestContext(
      std::unique_ptr<URLRequestContextConfig> context_config,
      std::unique_ptr<Callback> callback,
      scoped_refptr<base::SingleThreadTaskRunner> network_task_runner =
          nullptr);

  // Releases all resources for the request context and deletes the object.
  // Blocks until network thread is destroyed after running all pending tasks.
  virtual ~CronetURLRequestContext();

  // Called on init thread to initialize URLRequestContext.
  void InitRequestContextOnInitThread();

  // Posts a task that might depend on the context being initialized
  // to the network thread.
  void PostTaskToNetworkThread(const base::Location& posted_from,
                               base::OnceClosure callback);

  // Returns true if running on network thread.
  bool IsOnNetworkThread() const;

  // Returns net::URLRequestContext owned by |this|.
  net::URLRequestContext* GetURLRequestContext();

  // Returns a new instance of net::URLRequestContextGetter.
  // The net::URLRequestContext and base::SingleThreadTaskRunner that
  // net::URLRequestContextGetter returns are owned by |this|.
  net::URLRequestContextGetter* CreateURLRequestContextGetter();

  // TODO(xunjieli): Keep only one version of StartNetLog().

  // Starts NetLog logging to file. This can be called on any thread.
  // Return false if |file_name| cannot be opened.
  bool StartNetLogToFile(const std::string& file_name, bool log_all);

  // Starts NetLog logging to disk with a bounded amount of disk space. This
  // can be called on any thread.
  void StartNetLogToDisk(const std::string& dir_name,
                         bool log_all,
                         int max_size);

  // Stops NetLog logging to file. This can be called on any thread. This will
  // flush any remaining writes to disk.
  void StopNetLog();

  // Default net::LOAD flags used to create requests.
  int default_load_flags() const;

  // Configures the network quality estimator to observe requests to localhost,
  // to use smaller responses when estimating throughput, and to disable the
  // device offline checks when computing the effective connection type or when
  // writing the prefs. This should only be used for testing. This can be
  // called only after the network quality estimator has been enabled.
  void ConfigureNetworkQualityEstimatorForTesting(bool use_local_host_requests,
                                                  bool use_smaller_responses,
                                                  bool disable_offline_check);

  // Request that RTT and/or throughput observations should or should not be
  // provided by the network quality estimator.
  void ProvideRTTObservations(bool should);
  void ProvideThroughputObservations(bool should);

 private:
  friend class TestUtil;
  class ContextGetter;

  // NetworkTasks performs tasks on the network thread and owns objects that
  // live on the network thread.
  class NetworkTasks : public net::EffectiveConnectionTypeObserver,
                       public net::RTTAndThroughputEstimatesObserver,
                       public net::NetworkQualityEstimator::RTTObserver,
                       public net::NetworkQualityEstimator::ThroughputObserver {
   public:
    // Invoked off the network thread.
    NetworkTasks(std::unique_ptr<URLRequestContextConfig> config,
                 std::unique_ptr<CronetURLRequestContext::Callback> callback);
    // Invoked on the network thread.
    ~NetworkTasks() override;

    // Initializes |context_| on the network thread.
    void Initialize(
        scoped_refptr<base::SingleThreadTaskRunner> network_task_runner,
        scoped_refptr<base::SequencedTaskRunner> file_task_runner,
        std::unique_ptr<net::ProxyConfigService> proxy_config_service);

    // Runs a task that might depend on the context being initialized.
    void RunTaskAfterContextInit(
        base::OnceClosure task_to_run_after_context_init);

    // Configures the network quality estimator to observe requests to
    // localhost, to use smaller responses when estimating throughput, and to
    // disable the device offline checks when computing the effective connection
    // type or when writing the prefs. This should only be used for testing.
    void ConfigureNetworkQualityEstimatorForTesting(
        bool use_local_host_requests,
        bool use_smaller_responses,
        bool disable_offline_check);

    void ProvideRTTObservations(bool should);
    void ProvideThroughputObservations(bool should);

    // net::NetworkQualityEstimator::EffectiveConnectionTypeObserver
    // implementation.
    void OnEffectiveConnectionTypeChanged(
        net::EffectiveConnectionType effective_connection_type) override;

    // net::NetworkQualityEstimator::RTTAndThroughputEstimatesObserver
    // implementation.
    void OnRTTOrThroughputEstimatesComputed(
        base::TimeDelta http_rtt,
        base::TimeDelta transport_rtt,
        int32_t downstream_throughput_kbps) override;

    // net::NetworkQualityEstimator::RTTObserver implementation.
    void OnRTTObservation(int32_t rtt_ms,
                          const base::TimeTicks& timestamp,
                          net::NetworkQualityObservationSource source) override;

    // net::NetworkQualityEstimator::ThroughputObserver implementation.
    void OnThroughputObservation(
        int32_t throughput_kbps,
        const base::TimeTicks& timestamp,
        net::NetworkQualityObservationSource source) override;

    net::URLRequestContext* GetURLRequestContext();

    // Same as StartNetLogToDisk.
    void StartNetLogToBoundedFile(const std::string& dir_path,
                                  bool include_socket_bytes,
                                  int size);

    // Same as StartNetLogToFile, but called only on the network thread.
    void StartNetLog(const base::FilePath& file_path,
                     bool include_socket_bytes);

    // Stops NetLog logging.
    void StopNetLog();

    // Callback for StopObserving() that unblocks the client thread and
    // signals that it is safe to access the NetLog files.
    void StopNetLogCompleted();

    // Initializes Network Quality Estimator (NQE) prefs manager on network
    // thread.
    void InitializeNQEPrefs() const;

   private:
    friend class TestUtil;
    std::unique_ptr<base::DictionaryValue> GetNetLogInfo() const;

    std::unique_ptr<net::FileNetLogObserver> net_log_file_observer_;

    // A network quality estimator. This member variable has to be destroyed
    // after destroying |cronet_prefs_manager_|, which owns
    // NetworkQualityPrefsManager that weakly references
    // |network_quality_estimator_|.
    std::unique_ptr<net::NetworkQualityEstimator> network_quality_estimator_;

    // Manages the PrefService and all associated persistence managers
    // such as NetworkQualityPrefsManager, HostCachePersistenceManager, etc.
    // It should be destroyed before |network_quality_estimator_| and
    // after |context_|.
    std::unique_ptr<CronetPrefsManager> cronet_prefs_manager_;

    std::unique_ptr<net::URLRequestContext> context_;
    bool is_context_initialized_;

    // Context config is only valid until context is initialized.
    std::unique_ptr<URLRequestContextConfig> context_config_;

    // Effective experimental options. Kept for NetLog.
    std::unique_ptr<base::DictionaryValue> effective_experimental_options_;

    // A queue of tasks that need to be run after context has been initialized.
    base::queue<base::OnceClosure> tasks_waiting_for_context_;

    // Task runner that runs network tasks.
    scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;

    // Callback implemented by the client.
    std::unique_ptr<CronetURLRequestContext::Callback> callback_;

    THREAD_CHECKER(network_thread_checker_);
    DISALLOW_COPY_AND_ASSIGN(NetworkTasks);
  };

  scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner() const;

  // Gets the file thread. Create one if there is none.
  base::Thread* GetFileThread();

  const int default_load_flags_;

  // File thread should be destroyed last.
  std::unique_ptr<base::Thread> file_thread_;

  // |network_tasks_| is owned by |this|. It is created off the network thread,
  // but invoked and destroyed on network thread.
  NetworkTasks* network_tasks_;

  // Network thread is destroyed from client thread.
  std::unique_ptr<base::Thread> network_thread_;

  // Task runner that runs network tasks.
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(CronetURLRequestContext);
};

}  // namespace cronet

#endif  // COMPONENTS_CRONET_CRONET_URL_REQUEST_CONTEXT_H_
