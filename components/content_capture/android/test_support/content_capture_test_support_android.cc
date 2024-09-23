// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>

#include "base/android/jni_string.h"
#include "base/check_deref.h"
#include "base/json/json_reader.h"
#include "base/notreached.h"
#include "base/values.h"
#include "components/content_capture/browser/content_capture_receiver.h"
#include "components/content_capture/browser/onscreen_content_provider.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom.h"
#include "ui/gfx/geometry/size.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/content_capture/android/test_support/jni_headers/ContentCaptureTestSupport_jni.h"

namespace content_capture {

namespace {
blink::mojom::FaviconIconType ToType(std::string type) {
  if (type == "favicon")
    return blink::mojom::FaviconIconType::kFavicon;
  else if (type == "touch icon")
    return blink::mojom::FaviconIconType::kTouchIcon;
  else if (type == "touch precomposed icon")
    return blink::mojom::FaviconIconType::kTouchPrecomposedIcon;
  NOTREACHED_IN_MIGRATION();
  return blink::mojom::FaviconIconType::kInvalid;
}

}  // namespace

static void JNI_ContentCaptureTestSupport_DisableGetFaviconFromWebContents(
    JNIEnv* env) {
  ContentCaptureReceiver::DisableGetFaviconFromWebContentsForTesting();
}

static void JNI_ContentCaptureTestSupport_SimulateDidUpdateFaviconURL(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jwebContents,
    const base::android::JavaParamRef<jstring>& jfaviconJson) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jwebContents);
  CHECK(web_contents);
  OnscreenContentProvider* provider =
      OnscreenContentProvider::FromWebContents(web_contents);
  CHECK(provider);

  std::string json = base::android::ConvertJavaStringToUTF8(env, jfaviconJson);
  std::optional<base::Value> root = base::JSONReader::Read(json);
  CHECK(root);
  CHECK(root->is_list());
  std::vector<blink::mojom::FaviconURLPtr> favicon_urls;
  for (const base::Value& icon_val : root->GetList()) {
    const base::Value::Dict& icon = icon_val.GetDict();
    std::vector<gfx::Size> sizes;
    // The sizes is optional.
    if (const base::Value::List* icon_sizes = icon.FindList("sizes")) {
      for (const base::Value& size_val : CHECK_DEREF(icon_sizes)) {
        const base::Value::Dict& size = size_val.GetDict();

        const std::optional<int> width = size.FindInt("width");
        const std::optional<int> height = size.FindInt("height");
        CHECK(width);
        CHECK(height);
        sizes.emplace_back(width.value(), height.value());
      }
    }

    const std::string* url = icon.FindString("url");
    const std::string* type = icon.FindString("type");
    CHECK(url);
    CHECK(type);
    favicon_urls.push_back(blink::mojom::FaviconURL::New(
        GURL(*url), ToType(*type), sizes, /*is_default_icon=*/false));
  }
  CHECK(!favicon_urls.empty());
  provider->NotifyFaviconURLUpdatedForTesting(
      web_contents->GetPrimaryMainFrame(), favicon_urls);
}

}  // namespace content_capture
