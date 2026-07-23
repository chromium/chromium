// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBID_VERIFIED_ORIGIN_RESOLVER_H_
#define CHROME_BROWSER_WEBID_VERIFIED_ORIGIN_RESOLVER_H_

#include <string>
#include <utility>

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "url/origin.h"

namespace content::webid {

// Resolves a FedCM Identity Provider origin (`url::Origin`) to a verified
// Android app package name and bound service name.
//
// Uses the Android PackageManager (`queryIntentServices` for `"org.w3.FedCM"`)
// to discover candidate bound services, and Digital Asset Links
// (`ChromeOriginVerifier` for `RELATION_USE_AS_ORIGIN`) to verify that the
// package is authoritative for `origin`.
class VerifiedOriginResolver {
 public:
  enum class ResolveError {
    kNoServiceFound,
  };

  // Callback returning either a pair of (package_name, service_name) on
  // successful resolution, or an error.
  using Result =
      base::expected<std::pair<std::string, std::string>, ResolveError>;
  using ResolveCallback = base::OnceCallback<void(const Result&)>;

  VerifiedOriginResolver();
  ~VerifiedOriginResolver();

  VerifiedOriginResolver(const VerifiedOriginResolver&) = delete;
  VerifiedOriginResolver& operator=(const VerifiedOriginResolver&) = delete;

  // Resolves `origin` asynchronously to a verified Android bound service
  // (`package_name` and `service_name`). Invokes `callback` when complete.
  void Resolve(const url::Origin& origin, ResolveCallback callback);

  // Adds a Digital Asset Links verification override for testing.
  static void AddVerificationOverrideForTesting(const std::string& package_name,
                                                const url::Origin& origin);

  // Called from Java through JNI when resolution completes.
  void OnOriginResolved(JNIEnv* env,
                        const std::string& package_name,
                        const std::string& service_name);

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;
  ResolveCallback callback_;
};

}  // namespace content::webid

#endif  // CHROME_BROWSER_WEBID_VERIFIED_ORIGIN_RESOLVER_H_
