// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_PENGUIN_PENGUIN_CLIENT_H_
#define COMPONENTS_ACCESSIBILITY_PENGUIN_PENGUIN_CLIENT_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#endif

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace gfx {
class Rect;
}  // namespace gfx

namespace a11y {

// Use this callback to get only the main text from the API call.
using PenguinCompactResponseCallback =
    base::OnceCallback<void(const std::string&)>;

// Use this callback format to get all information from the API call, which
// will include response codes and all parts of the response (possibly
// multiple candidates, finish reason, safety ratings, etc.).
using PenguinFullResponseCallback =
    base::OnceCallback<void(int32_t response_code,
                            int32_t net_error_code,
                            const std::string& main_content)>;

// Use this callbacks to get the main text continuously from a Penguin API call.
using PenguinStreamingResponseCallback =
    base::RepeatingCallback<void(const std::string&)>;

class PenguinClient {
 public:
  static std::unique_ptr<PenguinClient> Create(content::BrowserContext* context,
                                               bool penguin_variation);

  explicit PenguinClient(content::BrowserContext* context);

  virtual ~PenguinClient();
  PenguinClient(const PenguinClient& client) = delete;
  PenguinClient& operator=(const PenguinClient& client) = delete;

  virtual void PerformAPICall(const std::string& image_data,
                              const std::string& text_input,
                              PenguinCompactResponseCallback callback);
  virtual void PerformAPICall(const std::string& image_data,
                              const std::string& text_input,
                              PenguinFullResponseCallback callback);

  virtual void PerformAPICall(content::WebContents* web_contents,
                              const std::string& text_input,
                              PenguinCompactResponseCallback callback);
  virtual void PerformAPICall(content::WebContents* web_contents,
                              const std::string& text_input,
                              PenguinFullResponseCallback callback);

  virtual void PerformAPICall(content::WebContents* web_contents,
                              const std::string& text_input,
                              PenguinCompactResponseCallback callback,
                              const gfx::Rect& source_rect);
  virtual void PerformAPICall(content::WebContents* web_contents,
                              const std::string& text_input,
                              PenguinFullResponseCallback callback,
                              const gfx::Rect& source_rect);

  virtual void PerformAPICall(const std::string& text_input,
                              PenguinCompactResponseCallback callback);
  virtual void PerformAPICall(const std::string& text_input,
                              PenguinFullResponseCallback callback);
  virtual void PerformAPICall(const std::string& text_input,
                              PenguinStreamingResponseCallback callback);

#if BUILDFLAG(IS_ANDROID)
  // Android equivalent methods that are called through JNI (cannot be
  // overloaded).
  virtual void PerformAPICall_var1(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& j_image_data,
      const base::android::JavaParamRef<jstring>& j_text_input,
      const base::android::JavaParamRef<jobject>& j_callback,
      jboolean j_include_full_response);

  virtual void PerformAPICall_var2(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_web_contents,
      const base::android::JavaParamRef<jstring>& j_text_input,
      const base::android::JavaParamRef<jobject>& j_callback,
      jboolean j_include_full_response);

  virtual void PerformAPICall_var3(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_web_contents,
      const base::android::JavaParamRef<jstring>& j_text_input,
      const base::android::JavaParamRef<jobject>& j_callback,
      const base::android::JavaParamRef<jobject>& j_source_rect,
      jboolean j_include_full_response);

  virtual void PerformAPICall_var4(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& j_text_input,
      const base::android::JavaParamRef<jobject>& j_callback,
      jboolean j_include_full_response);
#endif
};

}  // namespace a11y

#endif  // COMPONENTS_ACCESSIBILITY_PENGUIN_PENGUIN_CLIENT_H_
