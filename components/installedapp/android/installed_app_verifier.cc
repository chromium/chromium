// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "components/digital_asset_links/digital_asset_links_handler.h"
#include "components/installedapp/android/jni_headers/InstalledAppProviderImpl_jni.h"
#include "content/public/browser/android/browser_context_handle.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

void DidGetResult(
    std::unique_ptr<digital_asset_links::DigitalAssetLinksHandler> handler,
    base::OnceCallback<void(bool)> callback,
    digital_asset_links::RelationshipCheckResult result) {
  std::move(callback).Run(
      result == digital_asset_links::RelationshipCheckResult::kSuccess);
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

  std::string web_domain = ConvertJavaStringToUTF8(env, jwebDomain);
  std::string manifest_url = ConvertJavaStringToUTF8(env, jmanifestUrl);
  auto callback =
      base::BindOnce(&base::android::RunBooleanCallbackAndroid,
                     base::android::ScopedJavaGlobalRef<jobject>(jcallback));

  auto handler =
      std::make_unique<digital_asset_links::DigitalAssetLinksHandler>(
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
