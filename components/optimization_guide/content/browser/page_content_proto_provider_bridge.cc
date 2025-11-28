// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/callback_android.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/optimization_guide/content/browser/android/jni_headers/PageContentProtoProviderBridge_jni.h"

using ::base::android::ScopedJavaGlobalRef;
using ::jni_zero::JavaRef;

namespace optimization_guide::android {

static void JNI_PageContentProtoProviderBridge_GetAiPageContent(
    JNIEnv* env,
    content::WebContents* web_contents,
    const jni_zero::JavaParamRef<jobject>& j_callback) {
  blink::mojom::AIPageContentOptionsPtr extraction_options =
      optimization_guide::DefaultAIPageContentOptions(
          /* on_critical_path =*/true);
  optimization_guide::GetAIPageContent(
      web_contents, std::move(extraction_options),
      base::BindOnce(
          [](const JavaRef<jobject>& j_callback,
             optimization_guide::AIPageContentResultOrError result) -> void {
            if (!result.has_value()) {
              base::android::RunByteArrayCallbackAndroid(
                  j_callback, std::vector<uint8_t>());
              return;
            }
            std::string serialized_data;
            optimization_guide::proto::AnnotatedPageContent proto =
                std::move(result)->proto;
            proto.SerializeToString(&serialized_data);
            std::vector<uint8_t> serialized_data_vector(serialized_data.begin(),
                                                        serialized_data.end());

            base::android::RunByteArrayCallbackAndroid(j_callback,
                                                       serialized_data_vector);
          },
          ScopedJavaGlobalRef<jobject>(j_callback)));
}

}  // namespace optimization_guide::android

DEFINE_JNI(PageContentProtoProviderBridge)
