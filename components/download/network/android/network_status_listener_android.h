// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_NETWORK_ANDROID_NETWORK_STATUS_LISTENER_ANDROID_H_
#define COMPONENTS_DOWNLOAD_NETWORK_ANDROID_NETWORK_STATUS_LISTENER_ANDROID_H_

#include "components/download/network/network_status_listener.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"

namespace download {

// Android implementation of NetworkStatusListener.
// Backed by a Java class that can notify network change even when the app is
// in the background.
// The default implementation with net::NetworkChangeNotifier will NOT
// immediately notify changes when the app is in the background.
class NetworkStatusListenerAndroid : public NetworkStatusListener {
 public:
  NetworkStatusListenerAndroid();

  NetworkStatusListenerAndroid(const NetworkStatusListenerAndroid&) = delete;
  NetworkStatusListenerAndroid& operator=(const NetworkStatusListenerAndroid&) =
      delete;

  ~NetworkStatusListenerAndroid() override;

  // NetworkStatusListener implementation.
  void Start(NetworkStatusListener::Observer* observer) override;
  void Stop() override;
  network::mojom::ConnectionType GetConnectionType() override;

  void OnNetworkStatusReady(JNIEnv* env,
                            const base::android::JavaRef<jobject>& jobj,
                            jint connectionType);

  void NotifyNetworkChange(JNIEnv* env,
                           const base::android::JavaRef<jobject>& jobj,
                           jint connectionType);

 private:
  // The Java side object owned by this class.
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_NETWORK_ANDROID_NETWORK_STATUS_LISTENER_ANDROID_H_
