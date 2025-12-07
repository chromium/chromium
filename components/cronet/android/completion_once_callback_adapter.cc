// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/android/completion_once_callback_adapter.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "base/types/expected.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_errors.h"
#include "net/base/proxy_delegate.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_util.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/cronet/android/cronet_jni_headers/CompletionOnceCallback_jni.h"

namespace cronet {

base::android::ScopedJavaLocalRef<jobject>
CompletionOnceCallbackAdapter::Create(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    net::CompletionOnceCallback callback) {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_CompletionOnceCallback_Constructor(
      env, reinterpret_cast<long>(new CompletionOnceCallbackAdapter(
               task_runner, std::move(callback))));
}

CompletionOnceCallbackAdapter::CompletionOnceCallbackAdapter(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    net::CompletionOnceCallback callback)
    : callback_(std::move(callback)), task_runner_(task_runner) {}

CompletionOnceCallbackAdapter::~CompletionOnceCallbackAdapter() = default;

void CompletionOnceCallbackAdapter::Run(JNIEnv* env, net::Error result) {
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(std::move(callback_), result));
  delete this;
}
}  // namespace cronet

DEFINE_JNI(CompletionOnceCallback)
