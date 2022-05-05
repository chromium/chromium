// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/android/starter_delegate_android.h"

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/notreached.h"
#include "base/time/default_tick_clock.h"
#include "components/autofill_assistant/android/jni_headers/AssistantOnboardingHelperImpl_jni.h"
#include "components/autofill_assistant/android/jni_headers_public/Starter_jni.h"
#include "components/autofill_assistant/browser/android/client_android.h"
#include "components/autofill_assistant/browser/android/trigger_script_bridge_android.h"
#include "components/autofill_assistant/browser/android/ui_controller_android_utils.h"
#include "components/autofill_assistant/browser/assistant_field_trial_util.h"
#include "components/autofill_assistant/browser/public/runtime_manager_impl.h"
#include "components/autofill_assistant/browser/script_parameters.h"
#include "components/autofill_assistant/browser/website_login_manager_impl.h"
#include "components/version_info/android/channel_getter.h"
#include "components/version_info/channel.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "url/gurl.h"

using ::base::android::AttachCurrentThread;
using ::base::android::JavaObjectArrayReader;
using ::base::android::JavaParamRef;
using ::base::android::ScopedJavaGlobalRef;

namespace autofill_assistant {

static jlong JNI_Starter_FromWebContents(
    JNIEnv* env,
    const JavaParamRef<jobject>& jweb_contents,
    const JavaParamRef<jobject>& jstatic_dependencies) {
  auto* web_contents = content::WebContents::FromJavaWebContents(jweb_contents);
  CHECK(web_contents);

  auto dependencies = DependenciesAndroid::CreateFromJavaStaticDependencies(
      ScopedJavaGlobalRef<jobject>(env, jstatic_dependencies));
  StarterDelegateAndroid::CreateForWebContents(web_contents,
                                               std::move(dependencies));
  auto* tab_helper_android =
      StarterDelegateAndroid::FromWebContents(web_contents);
  Starter::CreateForWebContents(
      web_contents, tab_helper_android->GetWeakPtr(), ukm::UkmRecorder::Get(),
      RuntimeManagerImpl::GetForWebContents(web_contents)->GetWeakPtr(),
      base::DefaultTickClock::GetInstance());
  return reinterpret_cast<intptr_t>(tab_helper_android);
}

StarterDelegateAndroid::StarterDelegateAndroid(
    content::WebContents* web_contents,
    std::unique_ptr<DependenciesAndroid> dependencies)
    : content::WebContentsUserData<StarterDelegateAndroid>(*web_contents),
      dependencies_(std::move(dependencies)),
      website_login_manager_(std::make_unique<WebsiteLoginManagerImpl>(
          dependencies_->GetCommonDependencies()->GetPasswordManagerClient(
              web_contents),
          web_contents)) {
  // Create the AnnotateDomModelService when the browser starts, such that it
  // starts listening to model changes early enough.
  dependencies_->GetCommonDependencies()->GetOrCreateAnnotateDomModelService(
      web_contents->GetBrowserContext());
}

StarterDelegateAndroid::~StarterDelegateAndroid() = default;

void StarterDelegateAndroid::Attach(JNIEnv* env,
                                    const JavaParamRef<jobject>& jcaller) {
  Detach(env, jcaller);
  java_object_ = base::android::ScopedJavaGlobalRef<jobject>(jcaller);

  starter_ = Starter::FromWebContents(&GetWebContents())->GetWeakPtr();
  starter_->Init();
}

void StarterDelegateAndroid::Detach(JNIEnv* env,
                                    const JavaParamRef<jobject>& jcaller) {
  java_object_ = nullptr;
  java_dependencies_ = nullptr;
  if (starter_) {
    starter_->Init();
  }
  starter_ = nullptr;
}

std::unique_ptr<TriggerScriptCoordinator::UiDelegate>
StarterDelegateAndroid::CreateTriggerScriptUiDelegate() {
  CreateJavaDependenciesIfNecessary();
  return std::make_unique<TriggerScriptBridgeAndroid>(
      base::android::AttachCurrentThread(),
      GetWebContents().GetJavaWebContents(), java_dependencies_);
}

std::unique_ptr<ServiceRequestSender>
StarterDelegateAndroid::GetTriggerScriptRequestSenderToInject() {
  DCHECK(GetFeatureModuleInstalled());
  return ui_controller_android_utils::GetServiceRequestSenderToInject(
      base::android::AttachCurrentThread());
}

WebsiteLoginManager* StarterDelegateAndroid::GetWebsiteLoginManager() const {
  return website_login_manager_.get();
}

version_info::Channel StarterDelegateAndroid::GetChannel() const {
  return version_info::android::GetChannel();
}

bool StarterDelegateAndroid::GetFeatureModuleInstalled() const {
  return Java_Starter_getFeatureModuleInstalled(
      base::android::AttachCurrentThread());
}

void StarterDelegateAndroid::InstallFeatureModule(
    bool show_ui,
    base::OnceCallback<void(Metrics::FeatureModuleInstallation result)>
        callback) {
  DCHECK(java_object_);
  feature_module_installation_finished_callback_ = std::move(callback);
  Java_Starter_installFeatureModule(base::android::AttachCurrentThread(),
                                    java_object_, show_ui);
}

void StarterDelegateAndroid::OnFeatureModuleInstalled(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jint result) {
  DCHECK(feature_module_installation_finished_callback_);
  std::move(feature_module_installation_finished_callback_)
      .Run(static_cast<Metrics::FeatureModuleInstallation>(result));
}

void StarterDelegateAndroid::OnInteractabilityChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jboolean is_interactable) {
  if (!starter_) {
    return;
  }

  starter_->OnTabInteractabilityChanged(is_interactable);
}

void StarterDelegateAndroid::OnActivityAttachmentChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  java_dependencies_ = nullptr;
  if (!starter_) {
    return;
  }

  // Notify the starter. Some flows are only available in CCT or in regular tab,
  // so we need to cancel ongoing flows if they are no longer supported.
  starter_->OnDependenciesInvalidated();
}

bool StarterDelegateAndroid::GetIsFirstTimeUser() const {
  return Java_Starter_getIsFirstTimeUser(base::android::AttachCurrentThread());
}

void StarterDelegateAndroid::SetIsFirstTimeUser(bool first_time_user) {
  Java_Starter_setIsFirstTimeUser(base::android::AttachCurrentThread(),
                                  first_time_user);
}

bool StarterDelegateAndroid::GetOnboardingAccepted() const {
  return Java_Starter_getOnboardingAccepted(
      base::android::AttachCurrentThread());
}

void StarterDelegateAndroid::SetOnboardingAccepted(bool accepted) {
  Java_Starter_setOnboardingAccepted(base::android::AttachCurrentThread(),
                                     accepted);
}

void StarterDelegateAndroid::ShowOnboarding(
    bool use_dialog_onboarding,
    const TriggerContext& trigger_context,
    base::OnceCallback<void(bool shown, OnboardingResult result)> callback) {
  CreateJavaDependenciesIfNecessary();
  if (onboarding_finished_callback_) {
    DCHECK(false) << "onboarding requested while already being shown";
    std::move(callback).Run(false, OnboardingResult::DISMISSED);
    return;
  }
  onboarding_finished_callback_ = std::move(callback);

  std::vector<std::string> keys;
  std::vector<std::string> values;
  for (const auto& param : trigger_context.GetScriptParameters().ToProto()) {
    keys.emplace_back(param.name());
    values.emplace_back(param.value());
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_Starter_showOnboarding(env, java_object_, java_onboarding_helper_,
                              use_dialog_onboarding,
                              base::android::ConvertUTF8ToJavaString(
                                  env, trigger_context.GetExperimentIds()),
                              base::android::ToJavaArrayOfStrings(env, keys),
                              base::android::ToJavaArrayOfStrings(env, values));
}

void StarterDelegateAndroid::HideOnboarding() {
  if (!java_dependencies_) {
    return;
  }
  Java_Starter_hideOnboarding(base::android::AttachCurrentThread(),
                              java_object_, java_onboarding_helper_);
}

void StarterDelegateAndroid::OnOnboardingFinished(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jboolean shown,
    jint result) {
  // Currently, java may end up attempting to notify the native starter more
  // than once. The first notification is the user-triggered one, the others are
  // due to secondary effects, e.g., due to the dialog being hidden.
  // TODO(arbesser): fix this such that the callback is only called once.
  if (!onboarding_finished_callback_) {
    return;
  }
  std::move(onboarding_finished_callback_)
      .Run(shown, static_cast<OnboardingResult>(result));
}

bool StarterDelegateAndroid::GetProactiveHelpSettingEnabled() const {
  return Java_Starter_getProactiveHelpSettingEnabled(
      base::android::AttachCurrentThread());
}

void StarterDelegateAndroid::SetProactiveHelpSettingEnabled(bool enabled) {
  Java_Starter_setProactiveHelpSettingEnabled(
      base::android::AttachCurrentThread(), enabled);
}

bool StarterDelegateAndroid::GetMakeSearchesAndBrowsingBetterEnabled() const {
  if (!java_object_) {
    // Failsafe, should never happen.
    NOTREACHED();
    return false;
  }

  return Java_Starter_getMakeSearchesAndBrowsingBetterSettingEnabled(
      base::android::AttachCurrentThread(), java_object_);
}

bool StarterDelegateAndroid::GetIsLoggedIn() {
  return !dependencies_->GetCommonDependencies()
              ->GetSignedInEmail(&GetWebContents())
              .empty();
}

bool StarterDelegateAndroid::GetIsCustomTab() const {
  return dependencies_->GetPlatformDependencies()->IsCustomTab(
      GetWebContents());
}

bool StarterDelegateAndroid::GetIsWebLayer() const {
  return dependencies_->GetCommonDependencies()->IsWebLayer();
}

bool StarterDelegateAndroid::GetIsTabCreatedByGSA() const {
  if (!java_object_) {
    // Failsafe, should never happen.
    NOTREACHED();
    return false;
  }
  return Java_Starter_getIsTabCreatedByGSA(base::android::AttachCurrentThread(),
                                           java_object_);
}

void StarterDelegateAndroid::CreateJavaDependenciesIfNecessary() {
  if (java_dependencies_) {
    return;
  }

  JavaObjectArrayReader<jobject> array(
      Java_Starter_getOrCreateDependenciesAndOnboardingHelper(
          AttachCurrentThread(), java_object_));

  DCHECK_EQ(array.size(), 2);
  if (array.size() != 2) {
    return;
  }

  java_dependencies_ = *array.begin();
  java_onboarding_helper_ = *(++array.begin());
}

void StarterDelegateAndroid::Start(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaRef<jstring>& jexperiment_ids,
    const base::android::JavaRef<jobjectArray>& jparameter_names,
    const base::android::JavaRef<jobjectArray>& jparameter_values,
    const base::android::JavaRef<jobjectArray>& jdevice_only_parameter_names,
    const base::android::JavaRef<jobjectArray>& jdevice_only_parameter_values,
    const base::android::JavaRef<jstring>& jinitial_url) {
  DCHECK(starter_);
  auto trigger_context = ui_controller_android_utils::CreateTriggerContext(
      env, &GetWebContents(), jexperiment_ids, jparameter_names,
      jparameter_values, jdevice_only_parameter_names,
      jdevice_only_parameter_values,
      /* onboarding_shown = */ false, /* is_direct_action = */ false,
      jinitial_url, GetIsCustomTab());

  starter_->Start(std::move(trigger_context));
}

void StarterDelegateAndroid::StartScriptDefaultUi(
    GURL url,
    std::unique_ptr<TriggerContext> trigger_context,
    const absl::optional<TriggerScriptProto>& trigger_script) {
  CreateJavaDependenciesIfNecessary();
  ClientAndroid::CreateForWebContents(&GetWebContents(), java_dependencies_);
  auto* client_android = ClientAndroid::FromWebContents(&GetWebContents());
  DCHECK(client_android);

  JNIEnv* env = base::android::AttachCurrentThread();
  client_android->Start(
      url, std::move(trigger_context),
      ui_controller_android_utils::GetServiceToInject(env, client_android),
      Java_AssistantOnboardingHelperImpl_transferOnboardingOverlayCoordinator(
          env, java_onboarding_helper_),
      trigger_script);
}

bool StarterDelegateAndroid::IsRegularScriptRunning() const {
  const auto* client_android =
      ClientAndroid::FromWebContents(&GetWebContents());
  if (!client_android) {
    return false;
  }
  return client_android->IsRunning();
}

bool StarterDelegateAndroid::IsRegularScriptVisible() const {
  const auto* client_android =
      ClientAndroid::FromWebContents(&GetWebContents());
  if (!client_android) {
    return false;
  }
  return client_android->IsVisible();
}

std::unique_ptr<AssistantFieldTrialUtil>
StarterDelegateAndroid::CreateFieldTrialUtil() {
  return dependencies_->GetCommonDependencies()->CreateFieldTrialUtil();
}

bool StarterDelegateAndroid::IsAttached() {
  return !!java_object_;
}

const CommonDependencies* StarterDelegateAndroid::GetCommonDependencies() {
  return dependencies_->GetCommonDependencies();
}

const PlatformDependencies* StarterDelegateAndroid::GetPlatformDependencies() {
  return dependencies_->GetPlatformDependencies();
}

base::WeakPtr<StarterPlatformDelegate> StarterDelegateAndroid::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(StarterDelegateAndroid);

}  // namespace autofill_assistant
