// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_capture/android/test_support/jni_headers/ContentCaptureTestSupport_jni.h"

#include "base/android/jni_string.h"
#include "base/json/json_reader.h"
#include "base/notreached.h"
#include "base/values.h"
#include "components/content_capture/browser/content_capture_receiver.h"
#include "components/content_capture/browser/onscreen_content_provider.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom.h"
#include "ui/gfx/geometry/size.h"

namespace content_capture {

namespace {
blink::mojom::FaviconIconType ToType(std::string type) {
  if (type == "favicon")
    return blink::mojom::FaviconIconType::kFavicon;
  else if (type == "touch icon")
    return blink::mojom::FaviconIconType::kTouchIcon;
  else if (type == "touch precomposed icon")
    return blink::mojom::FaviconIconType::kTouchPrecomposedIcon;
  NOTREACHED();
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
  absl::optional<base::Value> root = base::JSONReader::Read(json);
  CHECK(root);
  CHECK(root->is_list());
  std::vector<blink::mojom::FaviconURLPtr> favicon_urls;
  for (const base::Value& icon : root->GetList()) {
    std::vector<gfx::Size> sizes;
    // The sizes is optional.
    if (auto* icon_sizes = icon.FindKey("sizes")) {
      for (const base::Value& size : icon_sizes->GetList()) {
        CHECK(size.FindKey("width"));
        CHECK(size.FindKey("height"));
        sizes.emplace_back(size.FindKey("width")->GetInt(),
                           size.FindKey("height")->GetInt());
      }
    }
    CHECK(icon.FindKey("url"));
    CHECK(icon.FindKey("type"));
    favicon_urls.push_back(blink::mojom::FaviconURL::New(
        GURL(*icon.FindKey("url")->GetIfString()),
        ToType(*icon.FindKey("type")->GetIfString()), sizes));
  }
  CHECK(!favicon_urls.empty());
  provider->NotifyFaviconURLUpdatedForTesting(
      web_contents->GetPrimaryMainFrame(), favicon_urls);
}

}  // namespace content_capture
