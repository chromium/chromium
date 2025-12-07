// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_ANDROID_COMPLETION_ONCE_CALLBACK_ADAPTER_H_
#define COMPONENTS_CRONET_ANDROID_COMPLETION_ONCE_CALLBACK_ADAPTER_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/task/single_thread_task_runner.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_errors.h"

namespace cronet {

// Adapter for org.chromium.net.impl.CompletionOnceCallback.
// TODO(https://crbug.com/442514750): Consider moving to
// base::android::ToJniCallback instead.
class CompletionOnceCallbackAdapter final {
 public:
  // Create a C++ CompletionOnceCallbackAdapter and a Java
  // CompletionOnceCallback. The lifetime of these two is tightly coupled:
  // CompletionOnceCallbackAdapter instances are owned by their Java
  // counterpart, CompletionOnceCallback. Ownership is then passed to
  // Cronet's embedder. This will be cleaned up only after the Java counterside
  // calls Run.
  static base::android::ScopedJavaLocalRef<jobject> Create(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      net::CompletionOnceCallback callback);

  // Runs, asynchronously, the associated callback onto the TaskRunner passed to
  // the constructor.
  // Warning: Object will self-destroy on return.
  void Run(JNIEnv* env, net::Error result);

 private:
  // Don't expose: creating a C++ CompletionOnceCallbackAdapter, without a Java
  // CompletionOnceCallback, is always wrong. See `Create`.
  explicit CompletionOnceCallbackAdapter(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      net::CompletionOnceCallback callback);
  ~CompletionOnceCallbackAdapter();

  net::CompletionOnceCallback callback_;
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

}  // namespace cronet

#endif  // COMPONENTS_CRONET_ANDROID_COMPLETION_ONCE_CALLBACK_ADAPTER_H_
