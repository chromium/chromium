// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CONTENT_ANDROID_TRANSLATE_UTILS_H_
#define COMPONENTS_TRANSLATE_CONTENT_ANDROID_TRANSLATE_UTILS_H_

#include <string>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"

namespace translate {

class TranslateInfoBarDelegate;

// This class is a wrapper information in Java format about languages used by
// the infobar.
struct JavaLanguageInfoWrapper {
  JavaLanguageInfoWrapper();
  ~JavaLanguageInfoWrapper();
  JavaLanguageInfoWrapper(const JavaLanguageInfoWrapper& other);

  base::android::ScopedJavaLocalRef<jobjectArray> java_languages;
  base::android::ScopedJavaLocalRef<jobjectArray> java_codes;
  base::android::ScopedJavaLocalRef<jintArray> java_hash_codes;
};

class TranslateUtils {
 public:
  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.translate
  // GENERATED_JAVA_PREFIX_TO_STRIP:OPTION_
  enum TranslateOption {
    OPTION_SOURCE_CODE,
    OPTION_TARGET_CODE,
    OPTION_ALWAYS_TRANSLATE,
    OPTION_NEVER_TRANSLATE,
    OPTION_NEVER_TRANSLATE_SITE
  };

  static base::android::ScopedJavaLocalRef<jintArray> GetJavaLanguageHashCodes(
      JNIEnv* env,
      std::vector<std::string>& language_codes);

  // An utility method that converts information about all translatable
  // languages to a Java format.
  static JavaLanguageInfoWrapper GetTranslateLanguagesInJavaFormat(
      JNIEnv* env,
      TranslateInfoBarDelegate* delegate);

  // An utility method that converts information about translatable user's
  // content languages codes to a Java format.
  static base::android::ScopedJavaLocalRef<jobjectArray>
  GetContentLanguagesInJavaFormat(JNIEnv* env,
                                  TranslateInfoBarDelegate* delegate);
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CONTENT_ANDROID_TRANSLATE_UTILS_H_
