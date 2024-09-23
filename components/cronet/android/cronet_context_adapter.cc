// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/android/cronet_context_adapter.h"
#include "components/cronet/android/proto/request_context_config.pb.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/cronet/android/cronet_library_loader.h"
#include "components/cronet/cronet_prefs_manager.h"
#include "components/cronet/host_cache_persistence_manager.h"
#include "components/cronet/url_request_context_config.h"
#include "components/metrics/library_support/histogram_manager.h"
#include "net/base/load_flags.h"
#include "net/base/logging_network_change_observer.h"
#include "net/base/net_errors.h"
#include "net/base/network_delegate_impl.h"
#include "net/base/url_util.h"
#include "net/cert/caching_cert_verifier.h"
#include "net/cert/cert_verifier.h"
#include "net/cookies/cookie_monster.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/log/file_net_log_observer.h"
#include "net/log/net_log_util.h"
#include "net/nqe/network_quality_estimator_params.h"
#include "net/proxy_resolution/proxy_config_service_android.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_interceptor.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/cronet/android/cronet_jni_headers/CronetUrlRequestContext_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace cronet {

CronetContextAdapter::CronetContextAdapter(
    std::unique_ptr<URLRequestContextConfig> context_config) {
  // Create context and pass ownership of |this| (self) to the context.
  context_ = new CronetContext(std::move(context_config),
                               base::WrapUnique<CronetContextAdapter>(this));
}

CronetContextAdapter::~CronetContextAdapter() = default;

void CronetContextAdapter::InitRequestContextOnInitThread(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller) {
  jcronet_url_request_context_.Reset(env, jcaller);
  context_->InitRequestContextOnInitThread();
}

void CronetContextAdapter::ConfigureNetworkQualityEstimatorForTesting(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    jboolean use_local_host_requests,
    jboolean use_smaller_responses,
    jboolean disable_offline_check) {
  context_->ConfigureNetworkQualityEstimatorForTesting(
      use_local_host_requests == JNI_TRUE, use_smaller_responses == JNI_TRUE,
      disable_offline_check == JNI_TRUE);
}

bool CronetContextAdapter::URLRequestContextExistsForTesting(
    net::handles::NetworkHandle network) {
  return context_->URLRequestContextExistsForTesting(network);  // IN-TEST
}

void CronetContextAdapter::ProvideRTTObservations(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    bool should) {
  context_->ProvideRTTObservations(should == JNI_TRUE);
}

void CronetContextAdapter::ProvideThroughputObservations(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    bool should) {
  context_->ProvideThroughputObservations(should == JNI_TRUE);
}

void CronetContextAdapter::OnInitNetworkThread() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CronetUrlRequestContext_initNetworkThread(env,
                                                 jcronet_url_request_context_);
}

void CronetContextAdapter::OnDestroyNetworkThread() {
  // The |context_| is destroyed.
  context_ = nullptr;
}

void CronetContextAdapter::OnEffectiveConnectionTypeChanged(
    net::EffectiveConnectionType effective_connection_type) {
  Java_CronetUrlRequestContext_onEffectiveConnectionTypeChanged(
      base::android::AttachCurrentThread(), jcronet_url_request_context_,
      effective_connection_type);
}

void CronetContextAdapter::OnRTTOrThroughputEstimatesComputed(
    int32_t http_rtt_ms,
    int32_t transport_rtt_ms,
    int32_t downstream_throughput_kbps) {
  Java_CronetUrlRequestContext_onRTTOrThroughputEstimatesComputed(
      base::android::AttachCurrentThread(), jcronet_url_request_context_,
      http_rtt_ms, transport_rtt_ms, downstream_throughput_kbps);
}

void CronetContextAdapter::OnRTTObservation(
    int32_t rtt_ms,
    int32_t timestamp_ms,
    net::NetworkQualityObservationSource source) {
  Java_CronetUrlRequestContext_onRttObservation(
      base::android::AttachCurrentThread(), jcronet_url_request_context_,
      rtt_ms, timestamp_ms, source);
}

void CronetContextAdapter::OnThroughputObservation(
    int32_t throughput_kbps,
    int32_t timestamp_ms,
    net::NetworkQualityObservationSource source) {
  Java_CronetUrlRequestContext_onThroughputObservation(
      base::android::AttachCurrentThread(), jcronet_url_request_context_,
      throughput_kbps, timestamp_ms, source);
}

void CronetContextAdapter::OnStopNetLogCompleted() {
  Java_CronetUrlRequestContext_stopNetLogCompleted(
      base::android::AttachCurrentThread(), jcronet_url_request_context_);
}

void CronetContextAdapter::Destroy(JNIEnv* env,
                                   const JavaParamRef<jobject>& jcaller) {
  // Deleting |context_| on client thread will post cleanup onto network thread,
  // which will in turn delete |this| on network thread.
  delete context_;
}

net::URLRequestContext* CronetContextAdapter::GetURLRequestContext(
    net::handles::NetworkHandle network) {
  return context_->GetURLRequestContext(network);
}

void CronetContextAdapter::PostTaskToNetworkThread(
    const base::Location& posted_from,
    base::OnceClosure callback) {
  context_->PostTaskToNetworkThread(posted_from, std::move(callback));
}

bool CronetContextAdapter::IsOnNetworkThread() const {
  return context_->IsOnNetworkThread();
}

bool CronetContextAdapter::StartNetLogToFile(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    const JavaParamRef<jstring>& jfile_name,
    jboolean jlog_all) {
  std::string file_name(
      base::android::ConvertJavaStringToUTF8(env, jfile_name));
  return context_->StartNetLogToFile(file_name, jlog_all == JNI_TRUE);
}

void CronetContextAdapter::StartNetLogToDisk(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    const JavaParamRef<jstring>& jdir_name,
    jboolean jlog_all,
    jint jmax_size) {
  std::string dir_name(base::android::ConvertJavaStringToUTF8(env, jdir_name));
  context_->StartNetLogToDisk(dir_name, jlog_all == JNI_TRUE, jmax_size);
}

void CronetContextAdapter::StopNetLog(JNIEnv* env,
                                      const JavaParamRef<jobject>& jcaller) {
  context_->StopNetLog();
}

void CronetContextAdapter::FlushWritePropertiesForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  context_->FlushWritePropertiesForTesting();  // IN-TEST
}

int CronetContextAdapter::default_load_flags() const {
  return context_->default_load_flags();
}

// Create a URLRequestContextConfig from the given parameters.
static jlong JNI_CronetUrlRequestContext_CreateRequestContextConfig(
    JNIEnv* env,
    const JavaParamRef<jbyteArray>& javaSerializedProto) {
  const int serializedProtoLength = env->GetArrayLength(javaSerializedProto);
  org::chromium::net::RequestContextConfigOptions configOptions;

  std::vector<uint8_t> serializedProto;

  base::android::JavaByteArrayToByteVector(env, javaSerializedProto,
                                           &serializedProto);

  if (!configOptions.ParseFromArray(serializedProto.data(),
                                    serializedProtoLength)) {
    return 0;
  }

  std::unique_ptr<URLRequestContextConfig> url_request_context_config =
      URLRequestContextConfig::CreateURLRequestContextConfig(
          configOptions.quic_enabled(), configOptions.http2_enabled(),
          configOptions.brotli_enabled(),
          static_cast<URLRequestContextConfig::HttpCacheType>(
              configOptions.http_cache_mode()),
          configOptions.http_cache_max_size(), configOptions.disable_cache(),
          configOptions.storage_path(),
          /* accept_languages */ std::string(), configOptions.user_agent(),
          configOptions.experimental_options(),
          base::WrapUnique(reinterpret_cast<net::CertVerifier*>(
              configOptions.mock_cert_verifier())),
          configOptions.enable_network_quality_estimator(),
          configOptions.bypass_public_key_pinning_for_local_trust_anchors(),
          configOptions.network_thread_priority() >= -20 &&
                  configOptions.network_thread_priority() <= 19
              ? std::optional<int>(configOptions.network_thread_priority())
              : std::optional<int>());
  return reinterpret_cast<jlong>(url_request_context_config.release());
}

// Add a QUIC hint to a URLRequestContextConfig.
static void JNI_CronetUrlRequestContext_AddQuicHint(
    JNIEnv* env,
    jlong jurl_request_context_config,
    const JavaParamRef<jstring>& jhost,
    jint jport,
    jint jalternate_port) {
  URLRequestContextConfig* config =
      reinterpret_cast<URLRequestContextConfig*>(jurl_request_context_config);
  config->quic_hints.push_back(
      std::make_unique<URLRequestContextConfig::QuicHint>(
          base::android::ConvertJavaStringToUTF8(env, jhost), jport,
          jalternate_port));
}

// Add a public key pin to URLRequestContextConfig.
// |jhost| is the host to apply the pin to.
// |jhashes| is an array of jbyte[32] representing SHA256 key hashes.
// |jinclude_subdomains| indicates if pin should be applied to subdomains.
// |jexpiration_time| is the time that the pin expires, in milliseconds since
// Jan. 1, 1970, midnight GMT.
static void JNI_CronetUrlRequestContext_AddPkp(
    JNIEnv* env,
    jlong jurl_request_context_config,
    const JavaParamRef<jstring>& jhost,
    const JavaParamRef<jobjectArray>& jhashes,
    jboolean jinclude_subdomains,
    jlong jexpiration_time) {
  URLRequestContextConfig* config =
      reinterpret_cast<URLRequestContextConfig*>(jurl_request_context_config);
  std::unique_ptr<URLRequestContextConfig::Pkp> pkp(
      new URLRequestContextConfig::Pkp(
          base::android::ConvertJavaStringToUTF8(env, jhost),
          jinclude_subdomains,
          base::Time::UnixEpoch() + base::Milliseconds(jexpiration_time)));
  for (auto bytes_array : jhashes.ReadElements<jbyteArray>()) {
    static_assert(std::is_pod<net::SHA256HashValue>::value,
                  "net::SHA256HashValue is not POD");
    static_assert(sizeof(net::SHA256HashValue) * CHAR_BIT == 256,
                  "net::SHA256HashValue contains overhead");
    if (env->GetArrayLength(bytes_array.obj()) !=
        sizeof(net::SHA256HashValue)) {
      LOG(ERROR) << "Unable to add public key hash value.";
      continue;
    }
    jbyte* bytes = env->GetByteArrayElements(bytes_array.obj(), nullptr);
    net::HashValue hash(*reinterpret_cast<net::SHA256HashValue*>(bytes));
    pkp->pin_hashes.push_back(hash);
    env->ReleaseByteArrayElements(bytes_array.obj(), bytes, JNI_ABORT);
  }
  config->pkp_list.push_back(std::move(pkp));
}

// Creates RequestContextAdater if config is valid URLRequestContextConfig,
// returns 0 otherwise.
static jlong JNI_CronetUrlRequestContext_CreateRequestContextAdapter(
    JNIEnv* env,
    jlong jconfig) {
  std::unique_ptr<URLRequestContextConfig> context_config(
      reinterpret_cast<URLRequestContextConfig*>(jconfig));

  CronetContextAdapter* context_adapter =
      new CronetContextAdapter(std::move(context_config));
  return reinterpret_cast<jlong>(context_adapter);
}

static ScopedJavaLocalRef<jbyteArray>
JNI_CronetUrlRequestContext_GetHistogramDeltas(JNIEnv* env) {
  std::vector<uint8_t> data;
  if (!metrics::HistogramManager::GetInstance()->GetDeltas(&data))
    return ScopedJavaLocalRef<jbyteArray>();
  return base::android::ToJavaByteArray(env, data);
}

}  // namespace cronet
