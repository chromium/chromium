// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/android/test/test_upload_data_stream_handler.h"

#include <stddef.h>
#include <string>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/functional/bind.h"
#include "components/cronet/android/test/cronet_test_util.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_with_source.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/cronet/android/cronet_test_apk_jni/TestUploadDataStreamHandler_jni.h"

using base::android::JavaParamRef;

namespace cronet {

static const size_t kReadBufferSize = 32768;

TestUploadDataStreamHandler::TestUploadDataStreamHandler(
    std::unique_ptr<net::UploadDataStream> upload_data_stream,
    JNIEnv* env,
    jobject jtest_upload_data_stream_handler,
    jlong jcontext_adapter)
    : init_callback_invoked_(false),
      read_callback_invoked_(false),
      bytes_read_(0),
      network_thread_(TestUtil::GetTaskRunner(jcontext_adapter)) {
  upload_data_stream_ = std::move(upload_data_stream);
  jtest_upload_data_stream_handler_.Reset(env,
                                          jtest_upload_data_stream_handler);
}

TestUploadDataStreamHandler::~TestUploadDataStreamHandler() {
}

void TestUploadDataStreamHandler::Destroy(JNIEnv* env) {
  DCHECK(!network_thread_->BelongsToCurrentThread());
  network_thread_->DeleteSoon(FROM_HERE, this);
}

void TestUploadDataStreamHandler::OnInitCompleted(int res) {
  DCHECK(network_thread_->BelongsToCurrentThread());
  init_callback_invoked_ = true;
  JNIEnv* env = base::android::AttachCurrentThread();
  cronet::Java_TestUploadDataStreamHandler_onInitCompleted(
      env, jtest_upload_data_stream_handler_, res);
}

void TestUploadDataStreamHandler::OnReadCompleted(int res) {
  DCHECK(network_thread_->BelongsToCurrentThread());
  read_callback_invoked_ = true;
  bytes_read_ = res;
  NotifyJavaReadCompleted();
}

void TestUploadDataStreamHandler::Init(JNIEnv* env) {
  DCHECK(!network_thread_->BelongsToCurrentThread());
  network_thread_->PostTask(
      FROM_HERE,
      base::BindOnce(&TestUploadDataStreamHandler::InitOnNetworkThread,
                     base::Unretained(this)));
}

void TestUploadDataStreamHandler::Read(JNIEnv* env) {
  DCHECK(!network_thread_->BelongsToCurrentThread());
  network_thread_->PostTask(
      FROM_HERE,
      base::BindOnce(&TestUploadDataStreamHandler::ReadOnNetworkThread,
                     base::Unretained(this)));
}

void TestUploadDataStreamHandler::Reset(JNIEnv* env) {
  DCHECK(!network_thread_->BelongsToCurrentThread());
  network_thread_->PostTask(
      FROM_HERE,
      base::BindOnce(&TestUploadDataStreamHandler::ResetOnNetworkThread,
                     base::Unretained(this)));
}

void TestUploadDataStreamHandler::CheckInitCallbackNotInvoked(JNIEnv* env) {
  DCHECK(!network_thread_->BelongsToCurrentThread());
  network_thread_->PostTask(
      FROM_HERE, base::BindOnce(&TestUploadDataStreamHandler::
                                    CheckInitCallbackNotInvokedOnNetworkThread,
                                base::Unretained(this)));
}

void TestUploadDataStreamHandler::CheckReadCallbackNotInvoked(JNIEnv* env) {
  DCHECK(!network_thread_->BelongsToCurrentThread());
  network_thread_->PostTask(
      FROM_HERE, base::BindOnce(&TestUploadDataStreamHandler::
                                    CheckReadCallbackNotInvokedOnNetworkThread,
                                base::Unretained(this)));
}

void TestUploadDataStreamHandler::InitOnNetworkThread() {
  DCHECK(network_thread_->BelongsToCurrentThread());
  init_callback_invoked_ = false;
  read_buffer_ = nullptr;
  bytes_read_ = 0;
  int res = upload_data_stream_->Init(
      base::BindOnce(&TestUploadDataStreamHandler::OnInitCompleted,
                     base::Unretained(this)),
      net::NetLogWithSource());
  JNIEnv* env = base::android::AttachCurrentThread();
  cronet::Java_TestUploadDataStreamHandler_onInitCalled(
      env, jtest_upload_data_stream_handler_, res);

  if (res == net::OK) {
    cronet::Java_TestUploadDataStreamHandler_onInitCompleted(
        env, jtest_upload_data_stream_handler_, res);
  }
}

void TestUploadDataStreamHandler::ReadOnNetworkThread() {
  DCHECK(network_thread_->BelongsToCurrentThread());
  read_callback_invoked_ = false;
  if (!read_buffer_.get())
    read_buffer_ = base::MakeRefCounted<net::IOBufferWithSize>(kReadBufferSize);

  int bytes_read = upload_data_stream_->Read(
      read_buffer_.get(), kReadBufferSize,
      base::BindOnce(&TestUploadDataStreamHandler::OnReadCompleted,
                     base::Unretained(this)));
  if (bytes_read == net::OK) {
    bytes_read_ = bytes_read;
    NotifyJavaReadCompleted();
  }
}

void TestUploadDataStreamHandler::ResetOnNetworkThread() {
  DCHECK(network_thread_->BelongsToCurrentThread());
  read_buffer_ = nullptr;
  bytes_read_ = 0;
  upload_data_stream_->Reset();
  JNIEnv* env = base::android::AttachCurrentThread();
  cronet::Java_TestUploadDataStreamHandler_onResetCompleted(
      env, jtest_upload_data_stream_handler_);
}

void TestUploadDataStreamHandler::CheckInitCallbackNotInvokedOnNetworkThread() {
  DCHECK(network_thread_->BelongsToCurrentThread());
  JNIEnv* env = base::android::AttachCurrentThread();
  cronet::Java_TestUploadDataStreamHandler_onCheckInitCallbackNotInvoked(
      env, jtest_upload_data_stream_handler_, !init_callback_invoked_);
}

void TestUploadDataStreamHandler::CheckReadCallbackNotInvokedOnNetworkThread() {
  DCHECK(network_thread_->BelongsToCurrentThread());
  JNIEnv* env = base::android::AttachCurrentThread();
  cronet::Java_TestUploadDataStreamHandler_onCheckReadCallbackNotInvoked(
      env, jtest_upload_data_stream_handler_, !read_callback_invoked_);
}

void TestUploadDataStreamHandler::NotifyJavaReadCompleted() {
  DCHECK(network_thread_->BelongsToCurrentThread());
  JNIEnv* env = base::android::AttachCurrentThread();
  std::string data_read = "";
  if (read_buffer_.get() && bytes_read_ > 0)
    data_read = std::string(read_buffer_->data(), bytes_read_);
  cronet::Java_TestUploadDataStreamHandler_onReadCompleted(
      env, jtest_upload_data_stream_handler_, bytes_read_,
      base::android::ConvertUTF8ToJavaString(env, data_read));
}

static jlong JNI_TestUploadDataStreamHandler_CreateTestUploadDataStreamHandler(
    JNIEnv* env,
    const JavaParamRef<jobject>& jtest_upload_data_stream_handler,
    jlong jupload_data_stream,
    jlong jcontext_adapter) {
  std::unique_ptr<net::UploadDataStream> upload_data_stream(
      reinterpret_cast<net::UploadDataStream*>(jupload_data_stream));
  TestUploadDataStreamHandler* handler = new TestUploadDataStreamHandler(
      std::move(upload_data_stream), env, jtest_upload_data_stream_handler,
      jcontext_adapter);
  return reinterpret_cast<jlong>(handler);
}

}  // namespace cronet
