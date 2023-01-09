// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_IOS_CRONET_ENVIRONMENT_H_
#define COMPONENTS_CRONET_IOS_CRONET_ENVIRONMENT_H_

#include <list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/strings/sys_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "components/cronet/url_request_context_config.h"
#include "components/cronet/version.h"
#include "net/cert/cert_verifier.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

namespace base {
class WaitableEvent;
}  // namespace base

namespace net {
class CookieStore;
class HttpNetworkSession;
class NetLog;
class FileNetLogObserver;
}  // namespace net

namespace cronet {
class CronetPrefsManager;

// CronetEnvironment contains all the network stack configuration
// and initialization.
class CronetEnvironment {
 public:
  using PkpVector = std::vector<std::unique_ptr<URLRequestContextConfig::Pkp>>;

  // A special thread priority value that indicates that the thread priority
  // should not be altered when a thread is created.
  static const double kKeepDefaultThreadPriority;

  // |user_agent| will be used to generate the user-agent if
  // |user_agent_partial| is true, or will be used as the complete user-agent
  // otherwise.
  CronetEnvironment(const std::string& user_agent, bool user_agent_partial);

  CronetEnvironment(const CronetEnvironment&) = delete;
  CronetEnvironment& operator=(const CronetEnvironment&) = delete;

  ~CronetEnvironment();

  // Starts this instance of Cronet environment.
  void Start();

  // The full user-agent.
  std::string user_agent();

  // Get global UMA histogram deltas.
  std::vector<uint8_t> GetHistogramDeltas();

  // Creates a new net log (overwrites existing file with this name). If
  // actively logging, this call is ignored.
  bool StartNetLog(base::FilePath::StringType file_name, bool log_bytes);
  // Stops logging and flushes file. If not currently logging this call is
  // ignored.
  void StopNetLog();

  void AddQuicHint(const std::string& host, int port, int alternate_port);

  // Setters and getters for |http2_enabled_|, |quic_enabled_|, and
  // |brotli_enabled| These only have any effect
  // before Start() is called.
  void set_http2_enabled(bool enabled) { http2_enabled_ = enabled; }
  void set_quic_enabled(bool enabled) { quic_enabled_ = enabled; }
  void set_brotli_enabled(bool enabled) { brotli_enabled_ = enabled; }

  bool http2_enabled() const { return http2_enabled_; }
  bool quic_enabled() const { return quic_enabled_; }
  bool brotli_enabled() const { return brotli_enabled_; }

  void set_accept_language(const std::string& accept_language) {
    accept_language_ = accept_language;
  }

  void set_mock_cert_verifier(
      std::unique_ptr<net::CertVerifier> mock_cert_verifier) {
    mock_cert_verifier_ = std::move(mock_cert_verifier);
  }

  void set_http_cache(URLRequestContextConfig::HttpCacheType http_cache) {
    http_cache_ = http_cache;
  }

  void set_experimental_options(const std::string& experimental_options) {
    experimental_options_ = experimental_options;
  }

  void SetHostResolverRules(const std::string& host_resolver_rules);

  void set_ssl_key_log_file_name(const std::string& ssl_key_log_file_name) {
    ssl_key_log_file_name_ = ssl_key_log_file_name;
  }

  void set_pkp_list(PkpVector pkp_list) { pkp_list_ = std::move(pkp_list); }

  void set_enable_public_key_pinning_bypass_for_local_trust_anchors(
      bool enable) {
    enable_pkp_bypass_for_local_trust_anchors_ = enable;
  }

  // Sets priority of the network thread. The |priority| should be a
  // floating point number between 0.0 to 1.0, where 1.0 is highest priority.
  void SetNetworkThreadPriority(double priority);

  // Returns the URLRequestContext associated with this object.
  net::URLRequestContext* GetURLRequestContext() const;

  // Return the URLRequestContextGetter associated with this object.
  net::URLRequestContextGetter* GetURLRequestContextGetter() const;

  // The methods below are used for testing.
  base::SingleThreadTaskRunner* GetFileThreadRunnerForTesting() const;
  base::SingleThreadTaskRunner* GetNetworkThreadRunnerForTesting() const;

 private:
  // Extends the base thread class to add the Cronet specific cleanup logic.
  class CronetNetworkThread : public base::Thread {
   public:
    CronetNetworkThread(const std::string& name,
                        cronet::CronetEnvironment* cronet_environment);

    CronetNetworkThread(const CronetNetworkThread&) = delete;
    CronetNetworkThread& operator=(const CronetNetworkThread&) = delete;

   protected:
    ~CronetNetworkThread() override;
    void CleanUp() override;

   private:
    cronet::CronetEnvironment* const cronet_environment_;
  };

  // Performs initialization tasks that must happen on the network thread.
  void InitializeOnNetworkThread();

  // Returns the task runner for the network thread.
  base::SingleThreadTaskRunner* GetNetworkThreadTaskRunner() const;

  // Runs a closure on the network thread.
  void PostToNetworkThread(const base::Location& from_here,
                           base::OnceClosure task);

  // Helper methods that start/stop net logging on the network thread.
  void StartNetLogOnNetworkThread(const base::FilePath&, bool log_bytes);
  void StopNetLogOnNetworkThread(base::WaitableEvent* log_stopped_event);

  base::Value GetNetLogInfo() const;

  // Returns the HttpNetworkSession object from the passed in
  // URLRequestContext or NULL if none exists.
  net::HttpNetworkSession* GetHttpNetworkSession(
      net::URLRequestContext* context);

  // Sets host resolver rules on the network_io_thread_.
  void SetHostResolverRulesOnNetworkThread(const std::string& rules,
                                           base::WaitableEvent* event);

  // Sets priority of the network thread. This method should only be called
  // on the network thread.
  void SetNetworkThreadPriorityOnNetworkThread(double priority);

  std::string getDefaultQuicUserAgentId() const;

  // Prepares the Cronet environment to be destroyed. The method must be
  // executed on the network thread. No other tasks should be posted to the
  // network thread after calling this method.
  void CleanUpOnNetworkThread();

  bool http2_enabled_;
  bool quic_enabled_;
  bool brotli_enabled_;
  std::string accept_language_;
  std::string experimental_options_;
  // Effective experimental options. Kept for NetLog.
  base::Value::Dict effective_experimental_options_;
  std::string ssl_key_log_file_name_;
  URLRequestContextConfig::HttpCacheType http_cache_;
  PkpVector pkp_list_;

  std::list<net::HostPortPair> quic_hints_;

  std::unique_ptr<base::Thread> network_io_thread_;
  std::unique_ptr<base::Thread> file_thread_;
  scoped_refptr<base::SequencedTaskRunner> pref_store_worker_pool_;
  std::unique_ptr<net::CertVerifier> mock_cert_verifier_;
  std::unique_ptr<net::CookieStore> cookie_store_;
  std::unique_ptr<net::URLRequestContext> main_context_;
  scoped_refptr<net::URLRequestContextGetter> main_context_getter_;
  std::string user_agent_;
  bool user_agent_partial_;
  net::NetLog* net_log_;
  std::unique_ptr<net::FileNetLogObserver> file_net_log_observer_;
  bool enable_pkp_bypass_for_local_trust_anchors_;
  double network_thread_priority_;
  std::unique_ptr<CronetPrefsManager> cronet_prefs_manager_;
};

}  // namespace cronet

#endif  // COMPONENTS_CRONET_IOS_CRONET_ENVIRONMENT_H_
