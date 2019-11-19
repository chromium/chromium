// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/android/cronet_upload_data_stream_adapter.h"

#include <memory>
#include <string>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/cronet/android/cronet_jni_headers/CronetUploadDataStream_jni.h"
#include "components/cronet/android/cronet_url_request_adapter.h"
#include "components/cronet/android/io_buffer_with_byte_buffer.h"

using base::android::JavaParamRef;

namespace cronet {

CronetUploadDataStreamAdapter::CronetUploadDataStreamAdapter(
    JNIEnv* env,
    jobject jupload_data_stream) {
  jupload_data_stream_.Reset(env, jupload_data_stream);
}

CronetUploadDataStreamAdapter::~CronetUploadDataStreamAdapter() {
}

void CronetUploadDataStreamAdapter::InitializeOnNetworkThread(
    base::WeakPtr<CronetUploadDataStream> upload_data_stream) {
  DCHECK(!upload_data_stream_);
  DCHECK(!network_task_runner_.get());

  upload_data_stream_ = upload_data_stream;
  network_task_runner_ = base::ThreadTaskRunnerHandle::Get();
  DCHECK(network_task_runner_);
}

void CronetUploadDataStreamAdapter::Read(net::IOBuffer* buffer, int buf_len) {
  DCHECK(upload_data_stream_);
  DCHECK(network_task_runner_);
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  DCHECK_GT(buf_len, 0);

  JNIEnv* env = base::android::AttachCurrentThread();
  // Allow buffer reuse if |buffer| and |buf_len| are exactly the same as the
  // ones used last time.
  if (!(buffer_ && buffer_->io_buffer()->data() == buffer->data() &&
        buffer_->io_buffer_len() == buf_len)) {
    buffer_ = std::make_unique<ByteBufferWithIOBuffer>(env, buffer, buf_len);
  }
  Java_CronetUploadDataStream_readData(env, jupload_data_stream_,
                                       buffer_->byte_buffer());
}

void CronetUploadDataStreamAdapter::Rewind() {
  DCHECK(upload_data_stream_);
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CronetUploadDataStream_rewind(env, jupload_data_stream_);
}

void CronetUploadDataStreamAdapter::OnUploadDataStreamDestroyed() {
  // If CronetUploadDataStream::InitInternal was never called,
  // |upload_data_stream_| and |network_task_runner_| will be NULL.
  DCHECK(!network_task_runner_ ||
         network_task_runner_->BelongsToCurrentThread());

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CronetUploadDataStream_onUploadDataStreamDestroyed(env,
                                                          jupload_data_stream_);
  // |this| is invalid here since the Java call above effectively destroys it.
}

void CronetUploadDataStreamAdapter::OnReadSucceeded(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    int bytes_read,
    bool final_chunk) {
  DCHECK(bytes_read > 0 || (final_chunk && bytes_read == 0));

  network_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&CronetUploadDataStream::OnReadSuccess,
                                upload_data_stream_, bytes_read, final_chunk));
}

void CronetUploadDataStreamAdapter::OnRewindSucceeded(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller) {
  network_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&CronetUploadDataStream::OnRewindSuccess,
                                upload_data_stream_));
}

void CronetUploadDataStreamAdapter::Destroy(JNIEnv* env) {
  delete this;
}

static jlong JNI_CronetUploadDataStream_AttachUploadDataToRequest(
    JNIEnv* env,
    const JavaParamRef<jobject>& jupload_data_stream,
    jlong jcronet_url_request_adapter,
    jlong jlength) {
  CronetURLRequestAdapter* request_adapter =
      reinterpret_cast<CronetURLRequestAdapter*>(jcronet_url_request_adapter);
  DCHECK(request_adapter != nullptr);

  CronetUploadDataStreamAdapter* adapter =
      new CronetUploadDataStreamAdapter(env, jupload_data_stream);

  std::unique_ptr<CronetUploadDataStream> upload_data_stream(
      new CronetUploadDataStream(adapter, jlength));

  request_adapter->SetUpload(std::move(upload_data_stream));

  return reinterpret_cast<jlong>(adapter);
}

static jlong JNI_CronetUploadDataStream_CreateAdapterForTesting(
    JNIEnv* env,
    const JavaParamRef<jobject>& jupload_data_stream) {
  CronetUploadDataStreamAdapter* adapter =
      new CronetUploadDataStreamAdapter(env, jupload_data_stream);
  return reinterpret_cast<jlong>(adapter);
}

static jlong JNI_CronetUploadDataStream_CreateUploadDataStreamForTesting(
    JNIEnv* env,
    const JavaParamRef<jobject>& jupload_data_stream,
    jlong jlength,
    jlong jadapter) {
  CronetUploadDataStreamAdapter* adapter =
      reinterpret_cast<CronetUploadDataStreamAdapter*>(jadapter);
  CronetUploadDataStream* upload_data_stream =
      new CronetUploadDataStream(adapter, jlength);
  return reinterpret_cast<jlong>(upload_data_stream);
}

}  // namespace cronet
