// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_relationship_verification/origin_verifier.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/functional/bind.h"
#include "components/content_relationship_verification/digital_asset_links_handler.h"
#include "content/public/browser/android/browser_context_handle.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"
#include "url/origin.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/content_relationship_verification/android/jni_headers/OriginVerifier_jni.h"

using base::android::AppendJavaStringArrayToStringVector;
using base::android::ConvertJavaStringToUTF8;
using base::android::JavaParamRef;
using base::android::JavaRef;
using content_relationship_verification::RelationshipCheckResult;

OriginVerifier::OriginVerifier(
    JNIEnv* env,
    const JavaRef<jobject>& obj,
    const JavaRef<jobject>& jbrowser_context_handle) {
  jobject_.Reset(obj);
  content::BrowserContext* context =
      content::BrowserContextFromJavaHandle(jbrowser_context_handle);
  DCHECK(context);
  url_loader_factory_ = context->GetDefaultStoragePartition()
                            ->GetURLLoaderFactoryForBrowserProcess();
}

OriginVerifier::~OriginVerifier() = default;

bool OriginVerifier::VerifyOrigin(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& j_package_name,
    const JavaParamRef<jobjectArray>& j_fingerprints,
    const JavaParamRef<jstring>& j_origin,
    const JavaParamRef<jstring>& j_relationship,
    const base::android::JavaRef<jobject>& jweb_contents) {
  if (!j_package_name || !j_fingerprints || !j_origin || !j_relationship) {
    return false;
  }
  raw_ptr<content::WebContents> web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  std::string package_name = ConvertJavaStringToUTF8(env, j_package_name);
  std::vector<std::string> fingerprints;
  AppendJavaStringArrayToStringVector(env, j_fingerprints, &fingerprints);
  std::string origin = ConvertJavaStringToUTF8(env, j_origin);
  std::string relationship = ConvertJavaStringToUTF8(env, j_relationship);

  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto asset_link_handler = std::make_unique<
      content_relationship_verification::DigitalAssetLinksHandler>(
      url_loader_factory_, web_contents);

  auto* asset_link_handler_ptr = asset_link_handler.get();

  return asset_link_handler_ptr->CheckDigitalAssetLinkRelationshipForAndroidApp(
      url::Origin::Create(GURL(origin)), relationship, std::move(fingerprints),
      package_name,
      base::BindOnce(&OriginVerifier::OnRelationshipCheckComplete,
                     base::Unretained(this), std::move(asset_link_handler),
                     origin));
}

void OriginVerifier::OnRelationshipCheckComplete(
    std::unique_ptr<content_relationship_verification::DigitalAssetLinksHandler>
        handler,
    const std::string& origin,
    RelationshipCheckResult result) {
  JNIEnv* env = base::android::AttachCurrentThread();

  auto j_origin = base::android::ConvertUTF8ToJavaString(env, origin);

  Java_OriginVerifier_onOriginVerificationResult(env, jobject_, j_origin,
                                                 static_cast<jint>(result));
}

// static
jlong OriginVerifier::Init(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobject>& jbrowser_context_handle) {
  OriginVerifier* native_verifier =
      new OriginVerifier(env, obj, jbrowser_context_handle);
  return reinterpret_cast<intptr_t>(native_verifier);
}

void OriginVerifier::Destroy(JNIEnv* env,
                             const base::android::JavaRef<jobject>& obj) {
  delete this;
}

static jlong JNI_OriginVerifier_Init(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobject>& jbrowser_context_handle) {
  return OriginVerifier::Init(env, obj, jbrowser_context_handle);
}
