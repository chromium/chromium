// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_RELATIONSHIP_VERIFICATION_ORIGIN_VERIFIER_H_
#define COMPONENTS_CONTENT_RELATIONSHIP_VERIFICATION_ORIGIN_VERIFIER_H_

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace content_relationship_verification {
enum class RelationshipCheckResult;
class DigitalAssetLinksHandler;
}  // namespace content_relationship_verification

// JNI bridge for OriginVerifier.java
class OriginVerifier {
 public:
  OriginVerifier(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& obj,
      const base::android::JavaRef<jobject>& jbrowser_context_handle);

  OriginVerifier(const OriginVerifier&) = delete;
  OriginVerifier& operator=(const OriginVerifier&) = delete;

  ~OriginVerifier();

  // Verify origin with the given parameters. No network requests can be made
  // if the params are null.
  bool VerifyOrigin(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& j_package_name,
      const base::android::JavaParamRef<jobjectArray>& j_fingerprints,
      const base::android::JavaParamRef<jstring>& j_origin,
      const base::android::JavaParamRef<jstring>& j_relationship,
      const base::android::JavaRef<jobject>& jweb_contents);

  static jlong Init(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& jbrowser_context_handle);

  void Destroy(JNIEnv* env, const base::android::JavaRef<jobject>& obj);

  static void ClearBrowsingData();

 private:
  void OnRelationshipCheckComplete(
      std::unique_ptr<
          content_relationship_verification::DigitalAssetLinksHandler> handler,
      const std::string& origin,
      content_relationship_verification::RelationshipCheckResult result);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  base::android::ScopedJavaGlobalRef<jobject> jobject_;
};

#endif  // COMPONENTS_CONTENT_RELATIONSHIP_VERIFICATION_ORIGIN_VERIFIER_H_
