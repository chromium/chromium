// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_ANDROID_CRONET_CONTEXT_ADAPTER_H_
#define COMPONENTS_CRONET_ANDROID_CRONET_CONTEXT_ADAPTER_H_

#include <jni.h>
#include <stdint.h>

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/threading/thread.h"
#include "components/cronet/cronet_context.h"
#include "components/prefs/json_pref_store.h"
#include "net/base/network_handle.h"
#include "net/nqe/effective_connection_type.h"
#include "net/nqe/effective_connection_type_observer.h"
#include "net/nqe/network_quality_estimator.h"
#include "net/nqe/network_quality_observation_source.h"
#include "net/nqe/rtt_throughput_estimates_observer.h"

namespace net {
class NetLog;
class URLRequestContext;
}  // namespace net

namespace cronet {
class TestUtil;

struct URLRequestContextConfig;

// Adapter between Java CronetUrlRequestContext and CronetContext.
class CronetContextAdapter : public CronetContext::Callback {
 public:
  explicit CronetContextAdapter(
      std::unique_ptr<URLRequestContextConfig> context_config);

  CronetContextAdapter(const CronetContextAdapter&) = delete;
  CronetContextAdapter& operator=(const CronetContextAdapter&) = delete;

  ~CronetContextAdapter() override;

  // Called on init Java thread to initialize URLRequestContext.
  void InitRequestContextOnInitThread(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller);

  // Releases all resources for the request context and deletes the object.
  // Blocks until network thread is destroyed after running all pending tasks.
  void Destroy(JNIEnv* env,
               const base::android::JavaParamRef<jobject>& jcaller);

  // Posts a task that might depend on the context being initialized
  // to the network thread.
  void PostTaskToNetworkThread(const base::Location& posted_from,
                               base::OnceClosure callback);

  bool IsOnNetworkThread() const;

  net::URLRequestContext* GetURLRequestContext(
      net::handles::NetworkHandle network =
          net::handles::kInvalidNetworkHandle);

  // TODO(xunjieli): Keep only one version of StartNetLog().

  // Starts NetLog logging to file. This can be called on any thread.
  // Return false if |jfile_name| cannot be opened.
  bool StartNetLogToFile(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& jcaller,
                         const base::android::JavaParamRef<jstring>& jfile_name,
                         jboolean jlog_all);

  // Starts NetLog logging to disk with a bounded amount of disk space. This
  // can be called on any thread.
  void StartNetLogToDisk(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& jcaller,
                         const base::android::JavaParamRef<jstring>& jdir_name,
                         jboolean jlog_all,
                         jint jmax_size);

  // Stops NetLog logging to file. This can be called on any thread. This will
  // flush any remaining writes to disk.
  void StopNetLog(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& jcaller);

  void FlushWritePropertiesForTesting(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller);

  // Default net::LOAD flags used to create requests.
  int default_load_flags() const;

  // Called on init Java thread to initialize URLRequestContext.
  void InitRequestContextOnInitThread();

  // Configures the network quality estimator to observe requests to localhost,
  // to use smaller responses when estimating throughput, and to disable the
  // device offline checks when computing the effective connection type or when
  // writing the prefs. This should only be used for testing. This can be
  // called only after the network quality estimator has been enabled.
  void ConfigureNetworkQualityEstimatorForTesting(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      jboolean use_local_host_requests,
      jboolean use_smaller_responses,
      jboolean disable_offline_check);

  bool URLRequestContextExistsForTesting(jlong network);

  // Request that RTT and/or throughput observations should or should not be
  // provided by the network quality estimator.
  void ProvideRTTObservations(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      bool should);
  void ProvideThroughputObservations(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      bool should);

  CronetContext* cronet_url_request_context() const { return context_; }

  // CronetContext::Callback
  void OnInitNetworkThread() override;
  void OnDestroyNetworkThread() override;
  void OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType effective_connection_type) override;
  void OnRTTOrThroughputEstimatesComputed(
      int32_t http_rtt_ms,
      int32_t transport_rtt_ms,
      int32_t downstream_throughput_kbps) override;
  void OnRTTObservation(int32_t rtt_ms,
                        int32_t timestamp_ms,
                        net::NetworkQualityObservationSource source) override;
  void OnThroughputObservation(
      int32_t throughput_kbps,
      int32_t timestamp_ms,
      net::NetworkQualityObservationSource source) override;
  void OnStopNetLogCompleted() override;

 private:
  friend class TestUtil;

  // Native Cronet URL Request Context.
  raw_ptr<CronetContext> context_;

  // Java object that owns this CronetContextAdapter.
  base::android::ScopedJavaGlobalRef<jobject> jcronet_url_request_context_;
};

}  // namespace cronet

#endif  // COMPONENTS_CRONET_ANDROID_CRONET_CONTEXT_ADAPTER_H_
