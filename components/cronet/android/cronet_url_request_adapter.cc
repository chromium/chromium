// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/android/cronet_url_request_adapter.h"

#include <limits>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "components/cronet/android/cronet_context_adapter.h"
#include "components/cronet/android/io_buffer_with_byte_buffer.h"
#include "components/cronet/android/shared_dictionary_with_byte_buffer.h"
#include "components/cronet/android/url_request_close_source.h"
#include "components/cronet/android/url_request_error.h"
#include "components/cronet/metrics_util.h"
#include "net/base/idempotency.h"
#include "net/base/load_flags.h"
#include "net/base/load_states.h"
#include "net/base/net_errors.h"
#include "net/base/proxy_server.h"
#include "net/base/request_priority.h"
#include "net/base/upload_data_stream.h"
#include "net/cert/cert_status_flags.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/ssl/ssl_info.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_packets.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request_context.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/cronet/android/cronet_jni_headers/CronetUrlRequest_jni.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;

namespace cronet {
namespace {

base::android::ScopedJavaLocalRef<jobjectArray> ConvertResponseHeadersToJava(
    JNIEnv* env,
    const net::HttpResponseHeaders* headers) {
  std::vector<std::string> response_headers;
  // Returns an empty array if |headers| is nullptr.
  if (headers != nullptr) {
    size_t iter = 0;
    std::string header_name;
    std::string header_value;
    while (headers->EnumerateHeaderLines(&iter, &header_name, &header_value)) {
      response_headers.push_back(header_name);
      response_headers.push_back(header_value);
    }
  }
  return base::android::ToJavaArrayOfStrings(env, response_headers);
}

}  // namespace

static jlong JNI_CronetUrlRequest_CreateRequestAdapter(
    JNIEnv* env,
    const JavaParamRef<jobject>& jurl_request,
    jlong jurl_request_context_adapter,
    const JavaParamRef<jstring>& jurl_string,
    jint jpriority,
    jboolean jdisable_cache,
    jboolean jdisable_connection_migration,
    jboolean jtraffic_stats_tag_set,
    jint jtraffic_stats_tag,
    jboolean jtraffic_stats_uid_set,
    jint jtraffic_stats_uid,
    jint jidempotency,
    const base::android::JavaParamRef<jbyteArray>& jdictionary_sha256_hash,
    const base::android::JavaParamRef<jobject>& jdictionary_byte_buffer,
    jint jdictionary_position,
    jint jdictionary_limit,
    const base::android::JavaParamRef<jstring>& jdictionary_id,
    jlong jnetwork_handle) {
  CHECK(jdictionary_id);
  CronetContextAdapter* context_adapter =
      reinterpret_cast<CronetContextAdapter*>(jurl_request_context_adapter);
  DCHECK(context_adapter);

  GURL url(base::android::ConvertJavaStringToUTF8(env, jurl_string));

  VLOG(1) << "New chromium network request_adapter: "
          << url.possibly_invalid_spec();

  CronetURLRequestAdapter* adapter = new CronetURLRequestAdapter(
      context_adapter, env, jurl_request, url,
      static_cast<net::RequestPriority>(jpriority), jdisable_cache,
      jdisable_connection_migration, jtraffic_stats_tag_set, jtraffic_stats_tag,
      jtraffic_stats_uid_set, jtraffic_stats_uid,
      static_cast<net::Idempotency>(jidempotency),
      SharedDictionaryWithByteBuffer::MaybeCreate(
          env,
          /*dictionary_sha256_hash=*/jdictionary_sha256_hash,
          /*dictionary_content_byte_buffer=*/jdictionary_byte_buffer,
          /*dictionary_content_position=*/jdictionary_position,
          /*dictionary_content_limit=*/jdictionary_limit,
          /*dictionary_id=*/jdictionary_id),
      jnetwork_handle);

  return reinterpret_cast<jlong>(adapter);
}

CronetURLRequestAdapter::CronetURLRequestAdapter(
    CronetContextAdapter* context,
    JNIEnv* env,
    jobject jurl_request,
    const GURL& url,
    net::RequestPriority priority,
    jboolean jdisable_cache,
    jboolean jdisable_connection_migration,
    jboolean jtraffic_stats_tag_set,
    jint jtraffic_stats_tag,
    jboolean jtraffic_stats_uid_set,
    jint jtraffic_stats_uid,
    net::Idempotency idempotency,
    scoped_refptr<net::SharedDictionary> shared_dictionary,
    jlong network)
    : request_(new CronetURLRequest(context->cronet_url_request_context(),
                                    base::WrapUnique<>(this),
                                    url,
                                    priority,
                                    jdisable_cache == JNI_TRUE,
                                    jdisable_connection_migration == JNI_TRUE,
                                    jtraffic_stats_tag_set == JNI_TRUE,
                                    jtraffic_stats_tag,
                                    jtraffic_stats_uid_set == JNI_TRUE,
                                    jtraffic_stats_uid,
                                    idempotency,
                                    shared_dictionary,
                                    network)) {
  owner_.Reset(env, jurl_request);
}

CronetURLRequestAdapter::~CronetURLRequestAdapter() {
}

jboolean CronetURLRequestAdapter::SetHttpMethod(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    const JavaParamRef<jstring>& jmethod) {
  std::string method(base::android::ConvertJavaStringToUTF8(env, jmethod));
  return request_->SetHttpMethod(method) ? JNI_TRUE : JNI_FALSE;
}

jboolean CronetURLRequestAdapter::AddRequestHeader(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    const JavaParamRef<jstring>& jname,
    const JavaParamRef<jstring>& jvalue) {
  std::string name(base::android::ConvertJavaStringToUTF8(env, jname));
  std::string value(base::android::ConvertJavaStringToUTF8(env, jvalue));
  return request_->AddRequestHeader(name, value) ? JNI_TRUE : JNI_FALSE;
}

void CronetURLRequestAdapter::SetUpload(
    std::unique_ptr<net::UploadDataStream> upload) {
  request_->SetUpload(std::move(upload));
}

void CronetURLRequestAdapter::Start(JNIEnv* env,
                                    const JavaParamRef<jobject>& jcaller) {
  request_->Start();
}

void CronetURLRequestAdapter::GetStatus(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    const JavaParamRef<jobject>& jstatus_listener) {
  base::android::ScopedJavaGlobalRef<jobject> status_listener_ref;
  status_listener_ref.Reset(env, jstatus_listener);
  request_->GetStatus(base::BindOnce(&CronetURLRequestAdapter::OnStatus,
                                     base::Unretained(this),
                                     status_listener_ref));
}

void CronetURLRequestAdapter::FollowDeferredRedirect(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller) {
  request_->FollowDeferredRedirect();
}

jboolean CronetURLRequestAdapter::ReadData(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    const JavaParamRef<jobject>& jbyte_buffer,
    jint jposition,
    jint jlimit) {
  DCHECK_LT(jposition, jlimit);

  IOBufferWithByteBuffer* read_buffer =
      new IOBufferWithByteBuffer(env, jbyte_buffer, jposition, jlimit);

  int remaining_capacity = jlimit - jposition;
  request_->ReadData(read_buffer, remaining_capacity);
  return JNI_TRUE;
}

void CronetURLRequestAdapter::Destroy(JNIEnv* env,
                                      const JavaParamRef<jobject>& jcaller,
                                      jboolean jsend_on_canceled) {
  // Destroy could be called from any thread, including network thread (if
  // posting task to executor throws an exception), but is posted, so |this|
  // is valid until calling task is complete. Destroy() is always called from
  // within a synchronized java block that guarantees no future posts to the
  // network thread with the adapter pointer.
  request_->Destroy(jsend_on_canceled == JNI_TRUE);
}

void CronetURLRequestAdapter::OnReceivedRedirect(
    const std::string& new_location,
    int http_status_code,
    const std::string& http_status_text,
    const net::HttpResponseHeaders* headers,
    bool was_cached,
    const std::string& negotiated_protocol,
    const std::string& proxy_server,
    int64_t received_byte_count) {
  JNIEnv* env = base::android::AttachCurrentThread();
  cronet::Java_CronetUrlRequest_onRedirectReceived(
      env, owner_, ConvertUTF8ToJavaString(env, new_location), http_status_code,
      ConvertUTF8ToJavaString(env, http_status_text),
      ConvertResponseHeadersToJava(env, headers),
      was_cached ? JNI_TRUE : JNI_FALSE,
      ConvertUTF8ToJavaString(env, negotiated_protocol),
      ConvertUTF8ToJavaString(env, proxy_server), received_byte_count);
}

void CronetURLRequestAdapter::OnResponseStarted(
    int http_status_code,
    const std::string& http_status_text,
    const net::HttpResponseHeaders* headers,
    bool was_cached,
    const std::string& negotiated_protocol,
    const std::string& proxy_server,
    int64_t received_byte_count) {
  JNIEnv* env = base::android::AttachCurrentThread();
  cronet::Java_CronetUrlRequest_onResponseStarted(
      env, owner_, http_status_code,
      ConvertUTF8ToJavaString(env, http_status_text),
      ConvertResponseHeadersToJava(env, headers),
      was_cached ? JNI_TRUE : JNI_FALSE,
      ConvertUTF8ToJavaString(env, negotiated_protocol),
      ConvertUTF8ToJavaString(env, proxy_server), received_byte_count);
}

void CronetURLRequestAdapter::OnReadCompleted(
    scoped_refptr<net::IOBuffer> buffer,
    int bytes_read,
    int64_t received_byte_count) {
  IOBufferWithByteBuffer* read_buffer =
      reinterpret_cast<IOBufferWithByteBuffer*>(buffer.get());
  JNIEnv* env = base::android::AttachCurrentThread();
  cronet::Java_CronetUrlRequest_onReadCompleted(
      env, owner_, read_buffer->byte_buffer(), bytes_read,
      read_buffer->initial_position(), read_buffer->initial_limit(),
      received_byte_count);
}

void CronetURLRequestAdapter::OnSucceeded(int64_t received_byte_count) {
  JNIEnv* env = base::android::AttachCurrentThread();
  cronet::Java_CronetUrlRequest_onSucceeded(env, owner_, received_byte_count);
}

void CronetURLRequestAdapter::OnError(int net_error,
                                      int quic_error,
                                      quic::ConnectionCloseSource source,
                                      const std::string& error_string,
                                      int64_t received_byte_count) {
  JNIEnv* env = base::android::AttachCurrentThread();
  cronet::Java_CronetUrlRequest_onError(
      env, owner_, NetErrorToUrlRequestError(net_error), net_error, quic_error,
      (int)NetSourceToJavaSource(source),
      ConvertUTF8ToJavaString(env, error_string), received_byte_count);
}

void CronetURLRequestAdapter::OnCanceled() {
  JNIEnv* env = base::android::AttachCurrentThread();
  cronet::Java_CronetUrlRequest_onCanceled(env, owner_);
}

void CronetURLRequestAdapter::OnDestroyed() {
  JNIEnv* env = base::android::AttachCurrentThread();
  cronet::Java_CronetUrlRequest_onNativeAdapterDestroyed(env, owner_);
  // |this| adapter will be destroyed by the owner after return from this call.
}

void CronetURLRequestAdapter::OnStatus(
    const base::android::ScopedJavaGlobalRef<jobject>& status_listener_ref,
    net::LoadState load_status) {
  JNIEnv* env = base::android::AttachCurrentThread();
  cronet::Java_CronetUrlRequest_onStatus(env, owner_, status_listener_ref,
                                         load_status);
}

void CronetURLRequestAdapter::OnMetricsCollected(
    const base::Time& start_time,
    const base::TimeTicks& start_ticks,
    const base::TimeTicks& dns_start,
    const base::TimeTicks& dns_end,
    const base::TimeTicks& connect_start,
    const base::TimeTicks& connect_end,
    const base::TimeTicks& ssl_start,
    const base::TimeTicks& ssl_end,
    const base::TimeTicks& send_start,
    const base::TimeTicks& send_end,
    const base::TimeTicks& push_start,
    const base::TimeTicks& push_end,
    const base::TimeTicks& receive_headers_end,
    const base::TimeTicks& request_end,
    bool socket_reused,
    int64_t sent_bytes_count,
    int64_t received_bytes_count,
    bool quic_connection_migration_attempted,
    bool quic_connection_migration_successful) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CronetUrlRequest_onMetricsCollected(
      env, owner_,
      metrics_util::ConvertTime(start_ticks, start_ticks, start_time),
      metrics_util::ConvertTime(dns_start, start_ticks, start_time),
      metrics_util::ConvertTime(dns_end, start_ticks, start_time),
      metrics_util::ConvertTime(connect_start, start_ticks, start_time),
      metrics_util::ConvertTime(connect_end, start_ticks, start_time),
      metrics_util::ConvertTime(ssl_start, start_ticks, start_time),
      metrics_util::ConvertTime(ssl_end, start_ticks, start_time),
      metrics_util::ConvertTime(send_start, start_ticks, start_time),
      metrics_util::ConvertTime(send_end, start_ticks, start_time),
      metrics_util::ConvertTime(push_start, start_ticks, start_time),
      metrics_util::ConvertTime(push_end, start_ticks, start_time),
      metrics_util::ConvertTime(receive_headers_end, start_ticks, start_time),
      metrics_util::ConvertTime(request_end, start_ticks, start_time),
      socket_reused ? JNI_TRUE : JNI_FALSE, sent_bytes_count,
      received_bytes_count,
      quic_connection_migration_attempted ? JNI_TRUE : JNI_FALSE,
      quic_connection_migration_successful ? JNI_TRUE : JNI_FALSE);
}

}  // namespace cronet
