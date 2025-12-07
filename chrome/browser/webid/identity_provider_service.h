// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBID_IDENTITY_PROVIDER_SERVICE_H_
#define CHROME_BROWSER_WEBID_IDENTITY_PROVIDER_SERVICE_H_

#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"

namespace content::webid {

// A C++ class to connect to Android apps through bound services.
// https://developer.android.com/develop/background-work/services/bound-services
//
// BoundServices are used to connect to an Identity Provider (IdP) app
// installed on the device to fetch identity information for FedCM.
//
// The browser uses an intent filter signature to discover the bound service
// exposed by the IdP app.
//
// https://developer.android.com/training/package-visibility/declaring#intent-filter-signature
//
// The IdP app is required to implement a bound service that responds to
// the "org.w3.FedCM" action in an intent filter and expose the service in
// their AndroidManifest.xml.
//
// For example:
//
// <service android:name=".FedCMService" android:exported="true">
//   <intent-filter>
//     <action android:name="org.w3.FedCM" />
//   </intent-filter>
// </service>
//
class IdentityProviderService {
 public:
  IdentityProviderService();
  ~IdentityProviderService();

  IdentityProviderService(const IdentityProviderService&) = delete;
  IdentityProviderService& operator=(const IdentityProviderService&) = delete;

  // Fetches data asynchronously. `callback` is called when the data is fetched.
  void Fetch(
      base::OnceCallback<void(const std::optional<std::string>&)> callback);

  // Connects to the service asynchronously. `callback` is called when the
  // service is connected.
  void Connect(const std::string& package_name,
               const std::string& service_name,
               base::OnceCallback<void(bool)> callback);

  // Disconnects from the service. `callback` is called when the service is
  // disconnected.
  void Disconnect(base::OnceCallback<void()> callback);

  // These callbacks need to be public as they are called from Java through JNI.
  void OnDataFetched(JNIEnv* env, std::optional<std::string> data);

  void OnConnected(JNIEnv* env, bool success);
  void OnDisconnected(JNIEnv* env);

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;
  base::OnceCallback<void(const std::optional<std::string>&)> callback_;
  base::OnceCallback<void(bool)> connect_callback_;
  base::OnceCallback<void()> disconnect_callback_;
};

}  // namespace content::webid

#endif  // CHROME_BROWSER_WEBID_IDENTITY_PROVIDER_SERVICE_H_
