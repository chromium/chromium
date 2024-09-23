// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "components/payments/content/has_enrolled_instrument_query_factory.h"
#include "components/payments/core/has_enrolled_instrument_query.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/payments/content/android/jni_headers/HasEnrolledInstrumentQuery_jni.h"

namespace payments {

// static
jboolean JNI_HasEnrolledInstrumentQuery_CanQuery(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jweb_contents,
    const base::android::JavaParamRef<jstring>& jtop_level_origin,
    const base::android::JavaParamRef<jstring>& jframe_origin,
    const base::android::JavaParamRef<jobject>& jquery_map) {
  auto* web_contents = content::WebContents::FromJavaWebContents(jweb_contents);
  if (!web_contents)
    return false;

  std::vector<std::string> method_identifiers =
      Java_HasEnrolledInstrumentQuery_getMethodIdentifiers(env, jquery_map);

  std::map<std::string, std::set<std::string>> query;
  for (const auto& method_identifier : method_identifiers) {
    std::set<std::string> method_specific_parameters = {
        Java_HasEnrolledInstrumentQuery_getStringifiedMethodData(
            env, jquery_map,
            base::android::ConvertUTF8ToJavaString(env, method_identifier))};
    query.insert(std::make_pair(method_identifier, method_specific_parameters));
  }

  return HasEnrolledInstrumentQueryFactory::GetInstance()
      ->GetForContext(web_contents->GetBrowserContext())
      ->CanQuery(
          GURL(base::android::ConvertJavaStringToUTF8(env, jtop_level_origin)),
          GURL(base::android::ConvertJavaStringToUTF8(env, jframe_origin)),
          query);
}

}  // namespace payments
