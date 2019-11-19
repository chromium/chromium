// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/network/android/network_status_listener_android.h"

#include "base/android/jni_android.h"
#include "base/trace_event/trace_event.h"
#include "components/download/network/jni_headers/NetworkStatusListenerAndroid_jni.h"

namespace download {

NetworkStatusListenerAndroid::NetworkStatusListenerAndroid() = default;

NetworkStatusListenerAndroid::~NetworkStatusListenerAndroid() = default;

void NetworkStatusListenerAndroid::NotifyNetworkChange(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jobj,
    jint connectionType) {
  DCHECK(observer_);
  using ConnectionType = network::mojom::ConnectionType;
  ConnectionType connection_type = static_cast<ConnectionType>(connectionType);
  observer_->OnNetworkChanged(connection_type);
}

void NetworkStatusListenerAndroid::Start(
    NetworkStatusListener::Observer* observer) {
  TRACE_EVENT0("download_service", "NetworkStatusListenerAndroid.Start");

  NetworkStatusListener::Start(observer);
  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_.Reset(env, Java_NetworkStatusListenerAndroid_create(
                           env, reinterpret_cast<intptr_t>(this))
                           .obj());
}

void NetworkStatusListenerAndroid::Stop() {
  NetworkStatusListener::Stop();
  Java_NetworkStatusListenerAndroid_clearNativePtr(
      base::android::AttachCurrentThread(), java_obj_);
}

network::mojom::ConnectionType
NetworkStatusListenerAndroid::GetConnectionType() {
  int connection_type =
      Java_NetworkStatusListenerAndroid_getCurrentConnectionType(
          base::android::AttachCurrentThread(), java_obj_);
  return static_cast<network::mojom::ConnectionType>(connection_type);
}

}  // namespace download
