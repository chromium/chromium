// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "components/back_forward_cache/back_forward_cache_disable.h"
#include "components/back_forward_cache/disabled_reason_id.h"
#include "components/payments/content/payment_request_web_contents_manager.h"
#include "components/payments/content/secure_payment_confirmation_transaction_mode.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/payments/content/android/jni_headers/PaymentRequestWebContentsData_jni.h"

namespace payments {
namespace android {

// static
static bool JNI_PaymentRequestWebContentsData_HadActivationlessShow(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jweb_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  CHECK(web_contents);

  return PaymentRequestWebContentsManager::GetOrCreateForWebContents(
             web_contents)
      ->HadActivationlessShow();
}

// static
static void JNI_PaymentRequestWebContentsData_RecordActivationlessShow(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jweb_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  CHECK(web_contents);

  PaymentRequestWebContentsManager::GetOrCreateForWebContents(web_contents)
      ->RecordActivationlessShow();
}

// static
static int32_t JNI_PaymentRequestWebContentsData_GetSPCTransactionMode(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jweb_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  CHECK(web_contents);

  return static_cast<int32_t>(
      PaymentRequestWebContentsManager::GetOrCreateForWebContents(web_contents)
          ->transaction_mode());
}

// static
static void JNI_PaymentRequestWebContentsData_DisableBFCacheForPreviousFrame(
    JNIEnv* env,
    int64_t native_navigation_handle) {
  auto* navigation_handle =
      reinterpret_cast<content::NavigationHandle*>(native_navigation_handle);
  if (!navigation_handle) {
    return;
  }
  content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(
      navigation_handle->GetPreviousRenderFrameHostId());
  if (rfh) {
    content::BackForwardCache::DisableForRenderFrameHost(
        rfh, back_forward_cache::DisabledReason(
                 back_forward_cache::DisabledReasonId::kModalDialog));
  }
}

}  // namespace android
}  // namespace payments

DEFINE_JNI(PaymentRequestWebContentsData)
