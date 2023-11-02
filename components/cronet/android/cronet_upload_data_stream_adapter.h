// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_ANDROID_CRONET_UPLOAD_DATA_STREAM_ADAPTER_H_
#define COMPONENTS_CRONET_ANDROID_CRONET_UPLOAD_DATA_STREAM_ADAPTER_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/cronet/cronet_upload_data_stream.h"
#include "net/base/io_buffer.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace cronet {
class ByteBufferWithIOBuffer;

// The Adapter holds onto a reference to the IOBuffer that is currently being
// written to in Java, so may not be deleted until any read operation in Java
// has completed.
//
// The Adapter is owned by the Java CronetUploadDataStream, and also owns a
// reference to it. The Adapter is only destroyed after the net::URLRequest
// destroys the C++ CronetUploadDataStream and the Java CronetUploadDataStream
// has no read operation pending, at which point it also releases its reference
// to the Java CronetUploadDataStream.
//
// Failures don't go back through the Adapter, but directly to the Java request
// object, since normally reads aren't allowed to fail during an upload.
class CronetUploadDataStreamAdapter : public CronetUploadDataStream::Delegate {
 public:
  CronetUploadDataStreamAdapter(JNIEnv* env, jobject jupload_data_stream);

  CronetUploadDataStreamAdapter(const CronetUploadDataStreamAdapter&) = delete;
  CronetUploadDataStreamAdapter& operator=(
      const CronetUploadDataStreamAdapter&) = delete;

  ~CronetUploadDataStreamAdapter() override;

  // CronetUploadDataStream::Delegate implementation.  Called on network thread.
  void InitializeOnNetworkThread(
      base::WeakPtr<CronetUploadDataStream> upload_data_stream) override;
  void Read(scoped_refptr<net::IOBuffer> buffer, int buf_len) override;
  void Rewind() override;
  void OnUploadDataStreamDestroyed() override;

  // Callbacks from Java, called on some Java thread.
  void OnReadSucceeded(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& obj,
                       int bytes_read,
                       bool final_chunk);
  void OnRewindSucceeded(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj);

  // Destroys |this|. Can be called from any thread, but needs to be protected
  // by the adapter lock.
  void Destroy(JNIEnv* env);

 private:
  // Initialized on construction, effectively constant.
  base::android::ScopedJavaGlobalRef<jobject> jupload_data_stream_;

  // These are initialized in InitializeOnNetworkThread, so are safe to access
  // during Java callbacks, which all happen after initialization.
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;
  base::WeakPtr<CronetUploadDataStream> upload_data_stream_;

  // Keeps the net::IOBuffer and Java ByteBuffer alive until the next Read().
  std::unique_ptr<ByteBufferWithIOBuffer> buffer_;
};

}  // namespace cronet

#endif  // COMPONENTS_CRONET_ANDROID_CRONET_UPLOAD_DATA_STREAM_ADAPTER_H_
