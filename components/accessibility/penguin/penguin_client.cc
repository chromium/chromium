// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility/penguin/penguin_client.h"

#include <memory>

#include "build/build_config.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_java_ref.h"
#include "components/accessibility/penguin_jni_headers/PenguinClient_jni.h"
#endif

namespace penguin {

// static
PenguinClient* PenguinClient::Create(content::BrowserContext* context) {
  return nullptr;
}

PenguinClient::PenguinClient(content::BrowserContext* context) {}

PenguinClient::~PenguinClient() = default;

void PenguinClient::PerformAPICall(const std::string& image_data,
                                   const std::string& text_input,
                                   PenguinCompactResponseCallback callback) {}

void PenguinClient::PerformAPICall(const std::string& image_data,
                                   const std::string& text_input,
                                   PenguinFullResponseCallback callback) {}

void PenguinClient::PerformAPICall(content::WebContents* web_contents,
                                   const std::string& text_input,
                                   PenguinCompactResponseCallback callback) {}

void PenguinClient::PerformAPICall(content::WebContents* web_contents,
                                   const std::string& text_input,
                                   PenguinFullResponseCallback callback) {}

void PenguinClient::PerformAPICall(content::WebContents* web_contents,
                                   const std::string& text_input,
                                   PenguinCompactResponseCallback callback,
                                   const gfx::Rect& source_rect) {}

void PenguinClient::PerformAPICall(content::WebContents* web_contents,
                                   const std::string& text_input,
                                   PenguinFullResponseCallback callback,
                                   const gfx::Rect& source_rect) {}

void PenguinClient::PerformAPICall(const std::string& text_input,
                                   PenguinCompactResponseCallback callback) {}

void PenguinClient::PerformAPICall(const std::string& text_input,
                                   PenguinFullResponseCallback callback) {}

#if BUILDFLAG(IS_ANDROID)

void PenguinClient::PerformAPICall_var1(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& j_image_data,
    const base::android::JavaParamRef<jstring>& j_text_input,
    const base::android::JavaParamRef<jobject>& j_callback,
    jboolean j_include_full_response) {}

void PenguinClient::PerformAPICall_var2(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_web_contents,
    const base::android::JavaParamRef<jstring>& j_text_input,
    const base::android::JavaParamRef<jobject>& j_callback,
    jboolean j_include_full_response) {}

void PenguinClient::PerformAPICall_var3(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_web_contents,
    const base::android::JavaParamRef<jstring>& j_text_input,
    const base::android::JavaParamRef<jobject>& j_callback,
    const base::android::JavaParamRef<jobject>& j_source_rect,
    jboolean j_include_full_response) {}

void PenguinClient::PerformAPICall_var4(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& j_text_input,
    const base::android::JavaParamRef<jobject>& j_callback,
    jboolean j_include_full_response) {}

#endif

}  // namespace penguin
