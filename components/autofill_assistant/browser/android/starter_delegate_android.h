// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ANDROID_STARTER_DELEGATE_ANDROID_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ANDROID_STARTER_DELEGATE_ANDROID_H_

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/android/dependencies.h"
#include "components/autofill_assistant/browser/assistant_field_trial_util.h"
#include "components/autofill_assistant/browser/metrics.h"
#include "components/autofill_assistant/browser/onboarding_result.h"
#include "components/autofill_assistant/browser/starter.h"
#include "components/autofill_assistant/browser/starter_platform_delegate.h"
#include "components/autofill_assistant/browser/trigger_context.h"
#include "components/autofill_assistant/browser/website_login_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill_assistant {

// C++ helper for the java-side |Starter|. Initializes the native-side |Starter|
// and serves as its Android platform delegate.
//
// This class is intended to be instantiated from Java via |FromWebContents|, at
// an appropriate time when the Java dependencies are ready and |Attach| will be
// called shortly after creation.
class StarterDelegateAndroid
    : public StarterPlatformDelegate,
      public content::WebContentsUserData<StarterDelegateAndroid> {
 public:
  ~StarterDelegateAndroid() override;
  StarterDelegateAndroid(const StarterDelegateAndroid&) = delete;
  StarterDelegateAndroid& operator=(const StarterDelegateAndroid&) = delete;

  // Attaches this instance to the java-side instance. Only call this with a
  // |jcaller| which is ready to serve requests by native!
  void Attach(JNIEnv* env, const base::android::JavaParamRef<jobject>& jcaller);

  // Detaches this instance from the java-side instance.
  void Detach(JNIEnv* env, const base::android::JavaParamRef<jobject>& jcaller);

  // Implements StarterPlatformDelegate:
  std::unique_ptr<TriggerScriptCoordinator::UiDelegate>
  CreateTriggerScriptUiDelegate() override;
  std::unique_ptr<ServiceRequestSender> GetTriggerScriptRequestSenderToInject()
      override;
  void StartScriptDefaultUi(
      GURL url,
      std::unique_ptr<TriggerContext> trigger_context,
      const absl::optional<TriggerScriptProto>& trigger_script) override;
  bool IsRegularScriptRunning() const override;
  bool IsRegularScriptVisible() const override;
  WebsiteLoginManager* GetWebsiteLoginManager() const override;
  version_info::Channel GetChannel() const override;
  bool GetFeatureModuleInstalled() const override;
  void InstallFeatureModule(
      bool show_ui,
      base::OnceCallback<void(Metrics::FeatureModuleInstallation result)>
          callback) override;
  bool GetIsFirstTimeUser() const override;
  void SetIsFirstTimeUser(bool first_time_user) override;
  bool GetOnboardingAccepted() const override;
  void SetOnboardingAccepted(bool accepted) override;
  void ShowOnboarding(
      bool use_dialog_onboarding,
      const TriggerContext& trigger_context,
      base::OnceCallback<void(bool shown, OnboardingResult result)> callback)
      override;
  void HideOnboarding() override;
  bool GetProactiveHelpSettingEnabled() const override;
  void SetProactiveHelpSettingEnabled(bool enabled) override;
  bool GetMakeSearchesAndBrowsingBetterEnabled() const override;
  bool GetIsLoggedIn() override;
  bool GetIsCustomTab() const override;
  bool GetIsWebLayer() const override;
  bool GetIsTabCreatedByGSA() const override;
  std::unique_ptr<AssistantFieldTrialUtil> CreateFieldTrialUtil() override;
  bool IsAttached() override;
  base::WeakPtr<StarterPlatformDelegate> GetWeakPtr() override;

  // Called by Java to start an autofill-assistant flow for an incoming intent.
  void Start(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      const base::android::JavaRef<jstring>& jexperiment_ids,
      const base::android::JavaRef<jobjectArray>& jparameter_names,
      const base::android::JavaRef<jobjectArray>& jparameter_values,
      const base::android::JavaRef<jobjectArray>& jdevice_only_parameter_names,
      const base::android::JavaRef<jobjectArray>& jdevice_only_parameter_values,
      const base::android::JavaRef<jstring>& jinitial_url);

  // Called by Java when the feature module installation has finished.
  void OnFeatureModuleInstalled(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      jint jresult);

  // Called by Java when the onboarding has finished.
  void OnOnboardingFinished(JNIEnv* env,
                            const base::android::JavaParamRef<jobject>& jcaller,
                            jboolean shown,
                            jint jresult);

  // Called by Java whenever the interactability of the tab has changed.
  void OnInteractabilityChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      jboolean is_interactable);

  // Called by Java when the activity attachment of the tab has changed, such as
  // when transitioning from a custom tab to a regular tab.
  void OnActivityAttachmentChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller);

 private:
  friend class content::WebContentsUserData<StarterDelegateAndroid>;
  StarterDelegateAndroid(content::WebContents* web_contents,
                         std::unique_ptr<Dependencies> dependencies);

  void CreateJavaDependenciesIfNecessary();

  WEB_CONTENTS_USER_DATA_KEY_DECL();
  base::WeakPtr<Starter> starter_;
  // Contains AssistantStaticDependencies which do not change.
  const std::unique_ptr<const Dependencies> dependencies_;
  // Can change based on activity attachment.
  base::android::ScopedJavaGlobalRef<jobject> java_dependencies_;

  base::android::ScopedJavaGlobalRef<jobject> java_object_;
  base::android::ScopedJavaGlobalRef<jobject> java_onboarding_helper_;
  std::unique_ptr<WebsiteLoginManager> website_login_manager_;
  base::OnceCallback<void(Metrics::FeatureModuleInstallation result)>
      feature_module_installation_finished_callback_;
  base::OnceCallback<void(bool shown, OnboardingResult result)>
      onboarding_finished_callback_;
  base::WeakPtrFactory<StarterDelegateAndroid> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ANDROID_STARTER_DELEGATE_ANDROID_H_
