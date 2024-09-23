// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webapps/web_app_offline.h"

#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/grit/components_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/webapps/browser/android/webapk/webapk_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/WebApkDataProvider_jni.h"

using base::android::ScopedJavaLocalRef;

namespace {

std::vector<std::string> GetOfflinePageInfoJava(
    const std::vector<int> requested_fields,
    const std::string& url,
    content::BrowserContext* browser_context,
    content::WebContents* web_contents) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobjectArray> java_result =
      Java_WebApkDataProvider_getOfflinePageInfo(
          env, base::android::ToJavaIntArray(env, requested_fields),
          base::android::ConvertUTF8ToJavaString(env, url),
          web_contents->GetJavaWebContents());
  std::vector<std::string> resource_strings;
  base::android::AppendJavaStringArrayToStringVector(env, java_result,
                                                     &resource_strings);
  return resource_strings;
}

}  // namespace

namespace web_app {

content::mojom::AlternativeErrorPageOverrideInfoPtr GetOfflinePageInfo(
    const GURL& url,
    content::RenderFrameHost* render_frame_host,
    content::BrowserContext* browser_context) {
  using webapps::WebApkDetailsForDefaultOfflinePage;
  const std::vector<int> fields = {
      (int)WebApkDetailsForDefaultOfflinePage::SHORT_NAME,
      (int)WebApkDetailsForDefaultOfflinePage::ICON};
  const std::vector<std::string> resource_strings = GetOfflinePageInfoJava(
      fields, url.spec(), browser_context,
      content::WebContents::FromRenderFrameHost(render_frame_host));

  if (fields.size() != resource_strings.size())
    return nullptr;

  auto alternative_error_page_info =
      content::mojom::AlternativeErrorPageOverrideInfo::New();

  base::Value::Dict dict;
  for (size_t i = 0; i < resource_strings.size(); ++i) {
    WebApkDetailsForDefaultOfflinePage field_id =
        (WebApkDetailsForDefaultOfflinePage)fields[i];
    switch (field_id) {
      case WebApkDetailsForDefaultOfflinePage::SHORT_NAME:
        dict.Set(error_page::kAppShortName, resource_strings[i]);
        break;
      case WebApkDetailsForDefaultOfflinePage::ICON:
        // Converting to GURL is necessary to correctly interpret the data url,
        // in case it contains embedded carriage returns, etc.
        dict.Set(error_page::kIconUrl, GURL(resource_strings[i]).spec());
        break;
    }
  }

  dict.Set(error_page::kMessage,
           l10n_util::GetStringUTF16(IDS_ERRORPAGES_HEADING_YOU_ARE_OFFLINE));
  dict.Set(error_page::kSupplementaryIcon, error_page::kOfflineIconId);
  alternative_error_page_info->alternative_error_page_params = std::move(dict);
  alternative_error_page_info->resource_id = IDR_WEBAPP_ERROR_PAGE_HTML;
  return alternative_error_page_info;
}

}  // namespace web_app
