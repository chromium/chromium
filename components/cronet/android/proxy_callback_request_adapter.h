// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_ANDROID_PROXY_CALLBACK_REQUEST_ADAPTER_H_
#define COMPONENTS_CRONET_ANDROID_PROXY_CALLBACK_REQUEST_ADAPTER_H_

#include <jni.h>

#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/task/single_thread_task_runner.h"
#include "net/base/proxy_delegate.h"
#include "net/http/http_request_headers.h"

namespace cronet {

// Adapter for org.chromium.net.impl.ProxyCallbackRequestImpl (implementation of
// org.chromium.net.Proxy.Callback.Request).
class ProxyCallbackRequestAdapter final {
 public:
  // Create a C++ ProxyCallbackRequestAdapter and a Java
  // ProxyCallbackRequestImpl. The lifetime of these two is tightly coupled:
  // ProxyCallbackRequestAdapter instances are owned by their Java counterpart,
  // ProxyCallbackRequestImpl. Ownership is then passed to Cronet's embedder.
  // This will be cleaned up only after Cronet's embedder calls
  // ProxyCallbackRequestImpl#{proceed, close}.
  static base::android::ScopedJavaLocalRef<jobject> CreateProxyCallbackRequest(
      net::ProxyDelegate::OnBeforeTunnelRequestCallback callback);

  // Object will self-destroy on return.
  bool Proceed(JNIEnv* env, std::vector<std::string> extra_headers);
  // Object will self-destroy on return.
  void Cancel(JNIEnv* env);

 private:
  // Don't expose: creating a C++ ProxyCallbackRequestAdapter, without a Java
  // ProxyCallbackRequestImpl, is always wrong. See
  // `CreateProxyCallbackRequest`.
  ProxyCallbackRequestAdapter(
      net::ProxyDelegate::OnBeforeTunnelRequestCallback callback);
  ~ProxyCallbackRequestAdapter();

  net::ProxyDelegate::OnBeforeTunnelRequestCallback callback_;
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;
};

}  // namespace cronet

#endif  // COMPONENTS_CRONET_ANDROID_PROXY_CALLBACK_REQUEST_ADAPTER_H_
