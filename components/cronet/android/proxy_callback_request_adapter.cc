// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/android/proxy_callback_request_adapter.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "base/types/expected.h"
#include "net/base/net_errors.h"
#include "net/base/proxy_delegate.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_util.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/cronet/android/cronet_jni_headers/ProxyCallbackRequestImpl_jni.h"

namespace cronet {

base::android::ScopedJavaLocalRef<jobject>
ProxyCallbackRequestAdapter::CreateProxyCallbackRequest(
    net::ProxyDelegate::OnBeforeTunnelRequestCallback callback) {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_ProxyCallbackRequestImpl_Constructor(
      env, reinterpret_cast<long>(
               new ProxyCallbackRequestAdapter(std::move(callback))));
}

ProxyCallbackRequestAdapter::ProxyCallbackRequestAdapter(
    net::ProxyDelegate::OnBeforeTunnelRequestCallback callback)
    : callback_(std::move(callback)),
      network_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {}

ProxyCallbackRequestAdapter::~ProxyCallbackRequestAdapter() = default;

bool ProxyCallbackRequestAdapter::Proceed(
    JNIEnv* env,
    std::vector<std::string> jextra_headers) {
  net::HttpRequestHeaders extra_headers;
  for (size_t i = 0; i < jextra_headers.size(); i += 2) {
    auto& key = jextra_headers[i];
    auto& value = jextra_headers[i + 1];
    if (!net::HttpUtil::IsValidHeaderName(key) ||
        !net::HttpUtil::IsValidHeaderValue(value)) {
      return false;
    }
    extra_headers.SetHeader(key, value);
  }

  network_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback_), std::move(extra_headers)));
  delete this;
  return true;
}

void ProxyCallbackRequestAdapter::Cancel(JNIEnv* env) {
  // TODO(https://crbug.com/422428959): Decide whether we want to propagate
  // org.chromium.net.Proxy.Callback canceling a tunnel establishment request as
  // something else (net::ERR_TUNNEL_CONNECTION_FAILED?
  // net::ERR_BLOCKED_BY_CLIENT? net::ERR_PROXY_TUNNEL_REQUEST_FAILED?). This is
  // currently not possible, as net::ProxyFallback::CanFalloverToNextProxy does
  // not try the next proxy for a lot of these errors, unless the chain is for
  // IP Protection. For the time being, we return another error for which the
  // next proxy is in the list is always attempted.
  network_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback_),
                                base::unexpected(net::ERR_CONNECTION_CLOSED)));
  delete this;
}

}  // namespace cronet
