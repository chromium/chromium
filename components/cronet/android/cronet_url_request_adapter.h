// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_ANDROID_CRONET_URL_REQUEST_ADAPTER_H_
#define COMPONENTS_CRONET_ANDROID_CRONET_URL_REQUEST_ADAPTER_H_

#include <jni.h>

#include <memory>
#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "components/cronet/cronet_url_request.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_types.h"
#include "url/gurl.h"

namespace net {
enum LoadState;
class UploadDataStream;
}  // namespace net

namespace cronet {

class CronetContextAdapter;
class TestUtil;

// An adapter from Java CronetUrlRequest object to native CronetURLRequest.
// Created and configured from a Java thread. Start, ReadData, and Destroy are
// posted to network thread and all callbacks into the Java CronetUrlRequest are
// done on the network thread. Java CronetUrlRequest is expected to initiate the
// next step like FollowDeferredRedirect, ReadData or Destroy. Public methods
// can be called on any thread.
class CronetURLRequestAdapter : public CronetURLRequest::Callback {
 public:
  // Bypasses cache if |jdisable_cache| is true. If context is not set up to
  // use cache, |jdisable_cache| has no effect. |jdisable_connection_migration|
  // causes connection migration to be disabled for this request if true. If
  // global connection migration flag is not enabled,
  // |jdisable_connection_migration| has no effect.
  CronetURLRequestAdapter(
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
      jlong network);

  CronetURLRequestAdapter(const CronetURLRequestAdapter&) = delete;
  CronetURLRequestAdapter& operator=(const CronetURLRequestAdapter&) = delete;

  ~CronetURLRequestAdapter() override;

  // Methods called prior to Start are never called on network thread.

  // Sets the request method GET, POST etc.
  jboolean SetHttpMethod(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& jcaller,
                         const base::android::JavaParamRef<jstring>& jmethod);

  // Adds a header to the request before it starts.
  jboolean AddRequestHeader(JNIEnv* env,
                            const base::android::JavaParamRef<jobject>& jcaller,
                            const base::android::JavaParamRef<jstring>& jname,
                            const base::android::JavaParamRef<jstring>& jvalue);

  // Adds a request body to the request before it starts.
  void SetUpload(std::unique_ptr<net::UploadDataStream> upload);

  // Starts the request.
  void Start(JNIEnv* env, const base::android::JavaParamRef<jobject>& jcaller);

  void GetStatus(JNIEnv* env,
                 const base::android::JavaParamRef<jobject>& jcaller,
                 const base::android::JavaParamRef<jobject>& jstatus_listener);

  // Follows redirect.
  void FollowDeferredRedirect(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller);

  // Reads more data.
  jboolean ReadData(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& jcaller,
                    const base::android::JavaParamRef<jobject>& jbyte_buffer,
                    jint jposition,
                    jint jcapacity);

  // Releases all resources for the request and deletes the object itself.
  // |jsend_on_canceled| indicates if Java onCanceled callback should be
  // issued to indicate when no more callbacks will be issued.
  void Destroy(JNIEnv* env,
               const base::android::JavaParamRef<jobject>& jcaller,
               jboolean jsend_on_canceled);

  // CronetURLRequest::Callback implementations:
  void OnReceivedRedirect(const std::string& new_location,
                          int http_status_code,
                          const std::string& http_status_text,
                          const net::HttpResponseHeaders* headers,
                          bool was_cached,
                          const std::string& negotiated_protocol,
                          const std::string& proxy_server,
                          int64_t received_byte_count) override;
  void OnResponseStarted(int http_status_code,
                         const std::string& http_status_text,
                         const net::HttpResponseHeaders* headers,
                         bool was_cached,
                         const std::string& negotiated_protocol,
                         const std::string& proxy_server,
                         int64_t received_byte_count) override;
  void OnReadCompleted(scoped_refptr<net::IOBuffer> buffer,
                       int bytes_read,
                       int64_t received_byte_count) override;
  void OnSucceeded(int64_t received_byte_count) override;
  void OnError(int net_error,
               int quic_error,
               quic::ConnectionCloseSource source,
               const std::string& error_string,
               int64_t received_byte_count) override;
  void OnCanceled() override;
  void OnDestroyed() override;
  void OnMetricsCollected(const base::Time& request_start_time,
                          const base::TimeTicks& request_start,
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
                          bool quic_connection_migration_successful) override;

  void OnStatus(
      const base::android::ScopedJavaGlobalRef<jobject>& status_listener_ref,
      net::LoadState load_status);

 private:
  friend class TestUtil;

  // Native Cronet URL Request that owns |this|.
  raw_ptr<CronetURLRequest> request_;

  // Java object that owns this CronetContextAdapter.
  base::android::ScopedJavaGlobalRef<jobject> owner_;
};

}  // namespace cronet

#endif  // COMPONENTS_CRONET_ANDROID_CRONET_URL_REQUEST_ADAPTER_H_
