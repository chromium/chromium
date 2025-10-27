// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_ANDROID_CRONET_BIDIRECTIONAL_STREAM_ADAPTER_H_
#define COMPONENTS_CRONET_ANDROID_CRONET_BIDIRECTIONAL_STREAM_ADAPTER_H_

#include <jni.h>

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "net/base/network_handle.h"
#include "net/http/bidirectional_stream.h"
#include "net/third_party/quiche/src/quiche/common/http/http_header_block.h"

namespace net {
struct BidirectionalStreamRequestInfo;
}  // namespace net

namespace cronet {

class CronetContextAdapter;
class IOBufferWithByteBuffer;

// Struct to hold the Cronet metrics that needs to be passed back to the java
// layer.
struct CronetMetrics final {
  // We can use a local reference here because in all uses of this struct the
  // ref is short-lived - the reference is passed to Java shortly after
  // construction and does not escape the stack.
  // org.chromium.net.impl.CronetMetrics
  base::android::ScopedJavaLocalRef<jobject> cronet_metrics;
  bool quic_connection_migration_attempted;
  bool quic_connection_migration_successful;
  // This is not ideal as it's preventing us from using designated initializers,
  // but sadly the Chromium "complex constructor has an inline body" check is
  // not giving us a choice.
  // https://www.chromium.org/developers/coding-style/chromium-style-checker-errors/#constructordestructor-errors
  CronetMetrics();
  CronetMetrics(const CronetMetrics& metrics);
  ~CronetMetrics();
};

// Convenient wrapper to hold Java references and data to represent the pending
// data to be written.
struct PendingWriteData {
  PendingWriteData(
      JNIEnv* env,
      const base::android::JavaRef<jobjectArray>& jwrite_buffer_list,
      const base::android::JavaRef<jintArray>& jwrite_buffer_pos_list,
      const base::android::JavaRef<jintArray>& jwrite_buffer_limit_list,
      jboolean jwrite_end_of_stream);

  PendingWriteData(const PendingWriteData&) = delete;
  PendingWriteData& operator=(const PendingWriteData&) = delete;

  ~PendingWriteData();

  // Arguments passed in from Java. Retain a global ref so they won't get GC-ed
  // until the corresponding onWriteCompleted is invoked.
  base::android::ScopedJavaGlobalRef<jobjectArray> jwrite_buffer_list;
  base::android::ScopedJavaGlobalRef<jintArray> jwrite_buffer_pos_list;
  base::android::ScopedJavaGlobalRef<jintArray> jwrite_buffer_limit_list;
  // A copy of the end of stream flag passed in from Java.
  jboolean jwrite_end_of_stream;
  // Every IOBuffer in |write_buffer_list| points to the memory owned by the
  // corresponding Java ByteBuffer in |jwrite_buffer_list|.
  std::vector<scoped_refptr<net::IOBuffer>> write_buffer_list;
  // A list of the length of each IOBuffer in |write_buffer_list|.
  std::vector<int> write_buffer_len_list;
};

// An adapter from Java BidirectionalStream object to net::BidirectionalStream.
// Created and configured from a Java thread. Start, ReadData, WritevData and
// Destroy can be called on any thread (including network thread), and post
// calls to corresponding {Start|ReadData|WritevData|Destroy}OnNetworkThread to
// the network thread. The object is always deleted on network thread. All
// callbacks into the Java BidirectionalStream are done on the network thread.
// Java BidirectionalStream is expected to initiate the next step like ReadData
// or Destroy. Public methods can be called on any thread.
class CronetBidirectionalStreamAdapter
    : public net::BidirectionalStream::Delegate {
 public:
  CronetBidirectionalStreamAdapter(
      CronetContextAdapter* context,
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jbidi_stream,
      bool jsend_request_headers_automatically,
      bool traffic_stats_tag_set,
      int32_t traffic_stats_tag,
      bool traffic_stats_uid_set,
      int32_t traffic_stats_uid,
      net::handles::NetworkHandle network);

  CronetBidirectionalStreamAdapter(const CronetBidirectionalStreamAdapter&) =
      delete;
  CronetBidirectionalStreamAdapter& operator=(
      const CronetBidirectionalStreamAdapter&) = delete;

  ~CronetBidirectionalStreamAdapter() override;

  // Validates method and headers, initializes and starts the request. If
  // |jend_of_stream| is true, then stream is half-closed after sending header
  // frame and no data is expected to be written.
  // Returns 0 if request is valid and started successfully,
  // Returns -1 if |jmethod| is not valid HTTP method name.
  // Returns position of invalid header value in |jheaders| if header name is
  // not valid.
  jint Start(JNIEnv* env,
             const base::android::JavaParamRef<jstring>& jurl,
             jint jpriority,
             const base::android::JavaParamRef<jstring>& jmethod,
             const base::android::JavaParamRef<jobjectArray>& jheaders,
             jboolean jend_of_stream);

  // Sends request headers to server.
  // When |send_request_headers_automatically_| is
  // false and OnStreamReady() is invoked with request_headers_sent = false,
  // headers will be combined with next WriteData/WritevData unless this
  // method is called first, in which case headers will be sent separately
  // without delay.
  // (This method cannot be called when |send_request_headers_automatically_| is
  // true nor when OnStreamReady() is invoked with request_headers_sent = true,
  // since headers have been sent by the stream when stream is negotiated
  // successfully.)
  void SendRequestHeaders(JNIEnv* env);

  // Reads more data into |jbyte_buffer| starting at |jposition| and not
  // exceeding |jlimit|. Arguments are preserved to ensure that |jbyte_buffer|
  // is not modified by the application during read.
  jboolean ReadData(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& jbyte_buffer,
                    jint jposition,
                    jint jlimit);

  // Writes more data from |jbyte_buffers|. For the i_th buffer in
  // |jbyte_buffers|, bytes to write start from i_th position in |jpositions|
  // and end at i_th limit in |jlimits|.
  // Arguments are preserved to ensure that |jbyte_buffer|
  // is not modified by the application during write. The |jend_of_stream| is
  // passed to remote to indicate end of stream.
  jboolean WritevData(
      JNIEnv* env,
      const base::android::JavaParamRef<jobjectArray>& jbyte_buffers,
      const base::android::JavaParamRef<jintArray>& jpositions,
      const base::android::JavaParamRef<jintArray>& jlimits,
      jboolean jend_of_stream);

  // Releases all resources for the request and deletes the object itself.
  // Responsible for collecting the metrics before destroying the object and
  // calling back into the java layer.
  void Destroy(JNIEnv* env);

 private:
  // net::BidirectionalStream::Delegate implementations:
  void OnStreamReady(bool request_headers_sent) override;
  void OnHeadersReceived(
      const quiche::HttpHeaderBlock& response_headers) override;
  void OnDataRead(int bytes_read) override;
  void OnDataSent() override;
  void OnTrailersReceived(const quiche::HttpHeaderBlock& trailers) override;
  void OnFailed(int error) override;

  void StartOnNetworkThread(
      std::unique_ptr<net::BidirectionalStreamRequestInfo> request_info);
  void SendRequestHeadersOnNetworkThread();
  void ReadDataOnNetworkThread(
      scoped_refptr<IOBufferWithByteBuffer> read_buffer,
      int buffer_size);
  void WritevDataOnNetworkThread(
      std::unique_ptr<PendingWriteData> pending_write_data);
  void DestroyOnNetworkThreadThenCallback();
  // Gets headers as a Java array.
  base::android::ScopedJavaLocalRef<jobjectArray> GetHeadersArray(
      JNIEnv* env,
      const quiche::HttpHeaderBlock& header_block);
  // Passes metrics to the Java layer if the stream existed (didn't fail to
  // start). Must be called on the network thread.
  std::optional<CronetMetrics> GetMetrics();
  const raw_ptr<CronetContextAdapter> context_;

  // Java object that owns this CronetBidirectionalStreamAdapter.
  base::android::ScopedJavaGlobalRef<jobject> owner_;
  const bool send_request_headers_automatically_;
  // Whether |traffic_stats_tag_| should be applied.
  const bool traffic_stats_tag_set_;
  // TrafficStats tag to apply to URLRequest.
  const int32_t traffic_stats_tag_;
  // Whether |traffic_stats_uid_| should be applied.
  const bool traffic_stats_uid_set_;
  // UID to be applied to URLRequest.
  const int32_t traffic_stats_uid_;
  // If not equal to net::handles::kInvalidNetworkHandle, the network to be used
  // to send this request.
  const net::handles::NetworkHandle network_;

  scoped_refptr<IOBufferWithByteBuffer> read_buffer_;
  std::unique_ptr<PendingWriteData> pending_write_data_;
  std::unique_ptr<net::BidirectionalStream> bidi_stream_;

  // Whether BidirectionalStream::Delegate::OnFailed callback is invoked.
  bool stream_failed_;
};

}  // namespace cronet

#endif  // COMPONENTS_CRONET_ANDROID_CRONET_BIDIRECTIONAL_STREAM_ADAPTER_H_
