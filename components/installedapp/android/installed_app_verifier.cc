// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "components/content_relationship_verification/digital_asset_links_handler.h"
#include "content/public/browser/android/browser_context_handle.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "url/gurl.h"
#include "url/origin.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/installedapp/android/jni_headers/InstalledAppProviderImpl_jni.h"

namespace {

void DidGetResult(
    std::unique_ptr<content_relationship_verification::DigitalAssetLinksHandler>
        handler,
    base::OnceCallback<void(bool)> callback,
    content_relationship_verification::RelationshipCheckResult result) {
  std::move(callback).Run(
      result ==
      content_relationship_verification::RelationshipCheckResult::kSuccess);
}

}  // namespace

namespace installedapp {

void JNI_InstalledAppProviderImpl_CheckDigitalAssetLinksRelationshipForWebApk(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jhandle,
    const base::android::JavaParamRef<jstring>& jwebDomain,
    const base::android::JavaParamRef<jstring>& jmanifestUrl,
    const base::android::JavaParamRef<jobject>& jcallback) {
  content::BrowserContext* browser_context =
      content::BrowserContextFromJavaHandle(jhandle);

  std::string web_domain =
      base::android::ConvertJavaStringToUTF8(env, jwebDomain);
  std::string manifest_url =
      base::android::ConvertJavaStringToUTF8(env, jmanifestUrl);
  auto callback =
      base::BindOnce(&base::android::RunBooleanCallbackAndroid,
                     base::android::ScopedJavaGlobalRef<jobject>(jcallback));

  auto handler = std::make_unique<
      content_relationship_verification::DigitalAssetLinksHandler>(
      browser_context->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess());
  auto* handler_ptr = handler.get();

  // |handler| is owned by the callback, so it will be valid until the execution
  // is over.
  handler_ptr->CheckDigitalAssetLinkRelationshipForWebApk(
      url::Origin::Create(GURL(web_domain)), manifest_url,
      base::BindOnce(&DidGetResult, std::move(handler), std::move(callback)));
}

}  // namespace installedapp
