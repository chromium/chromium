// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ANDROID_DEPENDENCIES_ANDROID_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ANDROID_DEPENDENCIES_ANDROID_H_

#include <memory>
#include "base/android/scoped_java_ref.h"
#include "base/strings/string_piece.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill_assistant/browser/assistant_field_trial_util.h"
#include "components/autofill_assistant/browser/common_dependencies.h"
#include "components/autofill_assistant/browser/platform_dependencies.h"
#include "components/autofill_assistant/content/browser/annotate_dom_model_service.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"

namespace autofill_assistant {

// Wrapper for all dependencies needed in android flows using legacy UI.
//
// Provides the right implementation of |CommonDependencies| and
// |PlatformDependencies| depending on platform and whether we are in Chrome or
// Weblayer.
class DependenciesAndroid {
 public:
  static std::unique_ptr<DependenciesAndroid> CreateFromJavaStaticDependencies(
      const base::android::JavaRef<jobject>& jstatic_dependencies);

  static std::unique_ptr<DependenciesAndroid> CreateFromJavaDependencies(
      const base::android::JavaRef<jobject>& jdependencies);

  base::android::ScopedJavaGlobalRef<jobject> GetJavaStaticDependencies() const;
  base::android::ScopedJavaGlobalRef<jobject> CreateInfoPageUtil() const;
  base::android::ScopedJavaGlobalRef<jobject> CreateAccessTokenUtil() const;
  base::android::ScopedJavaGlobalRef<jobject> CreateImageFetcher() const;
  base::android::ScopedJavaGlobalRef<jobject> CreateIconBridge() const;

  bool IsAccessibilityEnabled() const;

  virtual const CommonDependencies* GetCommonDependencies() const = 0;
  virtual const PlatformDependencies* GetPlatformDependencies() const = 0;

  virtual ~DependenciesAndroid();

 protected:
  DependenciesAndroid(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jstatic_dependencies);

  const base::android::ScopedJavaGlobalRef<jobject> jstatic_dependencies_;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ANDROID_DEPENDENCIES_ANDROID_H_
