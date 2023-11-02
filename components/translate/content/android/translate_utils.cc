// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/content/android/translate_utils.h"

#include <stddef.h>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/jni_weak_ref.h"
#include "components/metrics/metrics_log.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace translate {
JavaLanguageInfoWrapper::JavaLanguageInfoWrapper() = default;
JavaLanguageInfoWrapper::~JavaLanguageInfoWrapper() = default;
JavaLanguageInfoWrapper::JavaLanguageInfoWrapper(
    const JavaLanguageInfoWrapper&) = default;

ScopedJavaLocalRef<jintArray> TranslateUtils::GetJavaLanguageHashCodes(
    JNIEnv* env,
    std::vector<std::string>& language_codes) {
  std::vector<int> hashCodes;
  hashCodes.reserve(language_codes.size());
  for (auto code : language_codes) {
    hashCodes.push_back(metrics::MetricsLog::Hash(code));
  }
  return base::android::ToJavaIntArray(env, hashCodes);
}

JavaLanguageInfoWrapper TranslateUtils::GetTranslateLanguagesInJavaFormat(
    JNIEnv* env,
    TranslateInfoBarDelegate* delegate) {
  DCHECK(delegate);
  JavaLanguageInfoWrapper result;
  std::vector<std::u16string> languages;
  std::vector<std::string> codes;
  delegate->GetLanguagesNames(&languages);
  delegate->GetLanguagesCodes(&codes);
  result.java_languages = base::android::ToJavaArrayOfStrings(env, languages);
  result.java_codes = base::android::ToJavaArrayOfStrings(env, codes);
  result.java_hash_codes = GetJavaLanguageHashCodes(env, codes);
  return result;
}

ScopedJavaLocalRef<jobjectArray>
TranslateUtils::GetContentLanguagesInJavaFormat(
    JNIEnv* env,
    TranslateInfoBarDelegate* delegate) {
  DCHECK(delegate);
  std::vector<std::string> codes;
  delegate->GetContentLanguagesCodes(&codes);
  return base::android::ToJavaArrayOfStrings(env, codes);
}
}  // namespace translate
