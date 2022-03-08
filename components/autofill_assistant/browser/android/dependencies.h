// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ANDROID_DEPENDENCIES_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ANDROID_DEPENDENCIES_H_

#include <memory>
#include "base/android/scoped_java_ref.h"
#include "base/strings/string_piece.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill_assistant/browser/assistant_field_trial_util.h"
#include "components/autofill_assistant/content/browser/annotate_dom_model_service.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"

namespace autofill_assistant {

// Interface for platform delegates that provide platform-dependent features
// and dependencies to the starter.
class Dependencies {
 public:
  static std::unique_ptr<Dependencies> CreateFromJavaStaticDependencies(
      const base::android::JavaRef<jobject>& jstatic_dependencies);

  static std::unique_ptr<Dependencies> CreateFromJavaDependencies(
      const base::android::JavaRef<jobject>& jdependencies);

  base::android::ScopedJavaGlobalRef<jobject> GetJavaStaticDependencies() const;
  base::android::ScopedJavaGlobalRef<jobject> CreateInfoPageUtil() const;
  base::android::ScopedJavaGlobalRef<jobject> CreateAccessTokenUtil() const;
  base::android::ScopedJavaGlobalRef<jobject> CreateImageFetcher() const;
  base::android::ScopedJavaGlobalRef<jobject> CreateIconBridge() const;

  bool IsAccessibilityEnabled() const;

  virtual ~Dependencies();

  virtual std::unique_ptr<AssistantFieldTrialUtil> CreateFieldTrialUtil()
      const = 0;

  virtual autofill::PersonalDataManager* GetPersonalDataManager() const = 0;

  virtual password_manager::PasswordManagerClient* GetPasswordManagerClient(
      content::WebContents* web_contents) const = 0;

  virtual variations::VariationsService* GetVariationsService() const = 0;

  virtual std::string GetChromeSignedInEmailAddress(
      content::WebContents* web_contents) const = 0;

  virtual AnnotateDomModelService* GetOrCreateAnnotateDomModelService(
      content::BrowserContext* browser_context) const = 0;

  virtual bool IsCustomTab(const content::WebContents& web_contents) const = 0;

 protected:
  Dependencies(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jstatic_dependencies);

 private:
  const base::android::ScopedJavaGlobalRef<jobject> jstatic_dependencies_;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ANDROID_DEPENDENCIES_H_
