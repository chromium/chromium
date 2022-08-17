// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/android/client_android.h"

#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/locale_utils.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/json/json_writer.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/default_tick_clock.h"
#include "components/autofill_assistant/android/jni_headers/AssistantParseSingleTagXmlUtilWrapper_jni.h"
#include "components/autofill_assistant/android/jni_headers/AutofillAssistantClient_jni.h"
#include "components/autofill_assistant/android/jni_headers/AutofillAssistantDirectActionImpl_jni.h"
#include "components/autofill_assistant/browser/android/ui_controller_android_utils.h"
#include "components/autofill_assistant/browser/autofill_assistant_tts_controller.h"
#include "components/autofill_assistant/browser/controller.h"
#include "components/autofill_assistant/browser/display_strings_util.h"
#include "components/autofill_assistant/browser/features.h"
#include "components/autofill_assistant/browser/public/password_change/empty_website_login_manager_impl.h"
#include "components/autofill_assistant/browser/public/password_change/website_login_manager_impl.h"
#include "components/autofill_assistant/browser/public/ui_state.h"
#include "components/autofill_assistant/browser/service/access_token_fetcher.h"
#include "components/autofill_assistant/browser/switches.h"
#include "components/password_manager/content/browser/password_change_success_tracker_factory.h"
#include "components/password_manager/core/browser/password_change_success_tracker.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/version_info/android/channel_getter.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/tts_controller.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "url/gurl.h"

using ::base::android::AppendJavaStringArrayToStringVector;
using ::base::android::AttachCurrentThread;
using ::base::android::ConvertJavaStringToUTF8;
using ::base::android::ConvertUTF8ToJavaString;
using ::base::android::JavaParamRef;
using ::base::android::JavaRef;
using ::base::android::ScopedJavaGlobalRef;
using ::base::android::ScopedJavaLocalRef;
using ::base::android::ToJavaArrayOfStrings;

namespace autofill_assistant {
namespace {

// Experiment for "Data Input via QR Code Scanning". This is an Experiment id
// which is passed as part of the script parameters and is used to indicate
// whether QR Code Scan can be used for data input.
const char kDataInputViaQrCodeScanningExperiment[] = "4835818";

// Strings for Synthetic Field Trials.
const char kAutofillAssistantTtsTrialName[] = "AutofillAssistantEnableTtsParam";
const char kAutofillAssistantQrCodeScanningTrialName[] =
    "AutofillAssistantQrCodeScanning";
const char kEnabledGroupName[] = "Enabled";
const char kDisabledGroupName[] = "Disabled";

}  // namespace

static ScopedJavaLocalRef<jobject>
JNI_AutofillAssistantClient_CreateForWebContents(
    JNIEnv* env,
    const JavaParamRef<jobject>& jweb_contents,
    const JavaParamRef<jobject>& jdependencies) {
  auto* web_contents = content::WebContents::FromJavaWebContents(jweb_contents);
  ClientAndroid::CreateForWebContents(
      web_contents, ScopedJavaGlobalRef<jobject>(jdependencies));
  return ClientAndroid::FromWebContents(web_contents)->GetJavaObject();
}

static base::android::ScopedJavaLocalRef<jobject>
JNI_AutofillAssistantClient_FromWebContents(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jweb_contents) {
  auto* web_contents = content::WebContents::FromJavaWebContents(jweb_contents);
  auto* client_android = ClientAndroid::FromWebContents(web_contents);
  if (client_android == nullptr) {
    return nullptr;
  }

  return client_android->GetJavaObject();
}

static void JNI_AutofillAssistantClient_OnOnboardingUiChange(
    JNIEnv* env,
    const JavaParamRef<jobject>& jweb_contents,
    jboolean shown) {
  RuntimeManager* runtime_manager = RuntimeManager::GetForWebContents(
      content::WebContents::FromJavaWebContents(jweb_contents));
  if (runtime_manager)
    runtime_manager->SetUIState(shown ? UIState::kShown : UIState::kNotShown);
}

ClientAndroid::ClientAndroid(content::WebContents* web_contents,
                             const ScopedJavaGlobalRef<jobject>& jdependencies)
    : content::WebContentsUserData<ClientAndroid>(*web_contents),
      dependencies_(
          DependenciesAndroid::CreateFromJavaDependencies(jdependencies)),
      annotate_dom_model_service_(dependencies_->GetCommonDependencies()
                                      ->GetOrCreateAnnotateDomModelService(
                                          web_contents->GetBrowserContext())),
      jdependencies_(jdependencies),
      java_object_(Java_AutofillAssistantClient_Constructor(
          AttachCurrentThread(),
          reinterpret_cast<intptr_t>(this),
          dependencies_->CreateAccessTokenUtil())) {}

ClientAndroid::~ClientAndroid() {
  if (controller_ != nullptr && started_) {
    // In the case of an unexpected closing of the activity or tab, controller_
    // will not yet have been cleaned up (since that happens when a web
    // contents object gets destroyed).
    Metrics::RecordDropOut(Metrics::DropOutReason::CONTENT_DESTROYED, intent_);
  }

  Java_AutofillAssistantClient_clearNativePtr(AttachCurrentThread(),
                                              java_object_);
}

base::android::ScopedJavaLocalRef<jobject> ClientAndroid::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(java_object_);
}

bool ClientAndroid::IsRunning() const {
  return controller_ != nullptr;
}

bool ClientAndroid::IsVisible() const {
  return ui_controller_android_ != nullptr &&
         ui_controller_android_->IsAttached();
}

void ClientAndroid::Start(
    const GURL& url,
    std::unique_ptr<TriggerContext> trigger_context,
    std::unique_ptr<Service> test_service_to_inject,
    const base::android::JavaRef<jobject>& joverlay_coordinator,
    const absl::optional<TriggerScriptProto>& trigger_script) {
  // When Start() is called, AA_START should have been measured. From now on,
  // the client is responsible for keeping track of dropouts, so that for each
  // AA_START there's a corresponding dropout.
  started_ = true;

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> jaccount_name;
  if (trigger_context->GetScriptParameters().GetCallerEmail().has_value()) {
    jaccount_name = base::android::ConvertUTF8ToJavaString(
        env, trigger_context->GetScriptParameters().GetCallerEmail().value());
  }
  Java_AutofillAssistantClient_chooseAccountAsyncIfNecessary(
      base::android::AttachCurrentThread(), java_object_, jaccount_name);

  CreateController(std::move(test_service_to_inject), trigger_script);

  // If an overlay is already shown, then show the rest of the UI.
  if (joverlay_coordinator) {
    AttachUI(joverlay_coordinator);
  }

  // Register TTS Synthetic Field Trial.
  const bool enable_tts =
      trigger_context->GetScriptParameters().GetEnableTts().value_or(false);
  dependencies_->GetCommonDependencies()
      ->CreateFieldTrialUtil()
      ->RegisterSyntheticFieldTrial(
          kAutofillAssistantTtsTrialName,
          enable_tts ? kEnabledGroupName : kDisabledGroupName);

  // Register QR Code Scanning Synthetic Field Trial.
  const bool can_use_qr_code_scanning =
      trigger_context->GetScriptParameters().HasExperimentId(
          kDataInputViaQrCodeScanningExperiment);
  dependencies_->GetCommonDependencies()
      ->CreateFieldTrialUtil()
      ->RegisterSyntheticFieldTrial(
          kAutofillAssistantQrCodeScanningTrialName,
          can_use_qr_code_scanning ? kEnabledGroupName : kDisabledGroupName);

  DCHECK(!trigger_context->GetDirectAction());
  if (VLOG_IS_ON(2)) {
    DVLOG(2) << "Starting autofill assistant with parameters:";
    DVLOG(2) << "\ttarget_url: " << url;
    DVLOG(2) << "\texperiment_ids: " << trigger_context->GetExperimentIds();
    DVLOG(2) << "\tparameters:";
    auto parameters = trigger_context->GetScriptParameters().ToProto();
    for (const auto& param : parameters) {
      DVLOG(2) << "\t\t" << param.name() << ": " << param.value();
    }
  }
  controller_->Start(url, std::move(trigger_context));
}

void ClientAndroid::OnJavaDestroyUI(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  DestroyUI();
}

base::android::ScopedJavaLocalRef<jstring> ClientAndroid::GetPrimaryAccountName(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller) {
  return ConvertUTF8ToJavaString(env, GetSignedInEmail());
}

void ClientAndroid::OnAccessToken(JNIEnv* env,
                                  const JavaParamRef<jobject>& jcaller,
                                  jboolean success,
                                  const JavaParamRef<jstring>& access_token) {
  if (fetch_access_token_callback_) {
    std::move(fetch_access_token_callback_)
        .Run(success, base::android::ConvertJavaStringToUTF8(access_token));
  }
}

void ClientAndroid::FetchWebsiteActions(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jstring>& jexperiment_ids,
    const base::android::JavaParamRef<jobjectArray>& jparameter_names,
    const base::android::JavaParamRef<jobjectArray>& jparameter_values,
    const base::android::JavaParamRef<jobject>& jcallback) {
  if (!controller_) {
    CreateController(ui_controller_android_utils::GetServiceToInject(env, this),
                     /* trigger_script= */ absl::nullopt);
  }

  base::android::ScopedJavaGlobalRef<jobject> scoped_jcallback(env, jcallback);
  controller_->Track(
      ui_controller_android_utils::CreateTriggerContext(
          env, GetWebContents(), jexperiment_ids, jparameter_names,
          jparameter_values, /* jdevice_only_parameter_names= */
          base::android::JavaParamRef<jobjectArray>(nullptr),
          /* jdevice_only_parameter_values= */
          base::android::JavaParamRef<jobjectArray>(nullptr),
          /* onboarding_shown = */ false,
          /* is_direct_action = */ true,
          /* jinitial_url = */ nullptr,
          /* is_custom_tab = */
          dependencies_->GetPlatformDependencies()->IsCustomTab(
              *GetWebContents())),
      base::BindOnce(&ClientAndroid::OnFetchWebsiteActions,
                     weak_ptr_factory_.GetWeakPtr(), scoped_jcallback));
}

bool ClientAndroid::HasRunFirstCheck(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) const {
  return controller_ != nullptr && controller_->HasRunFirstCheck();
}

base::android::ScopedJavaLocalRef<jobjectArray>
ClientAndroid::GetDirectActionsAsJavaArrayOfStrings(JNIEnv* env) const {
  // Using a set here helps remove duplicates.
  std::set<std::string> names;

  if (!controller_) {
    return base::android::ToJavaArrayOfStrings(
        env, std::vector<std::string>(names.begin(), names.end()));
  }

  for (const ScriptHandle& script : controller_->GetDirectActionScripts()) {
    for (const std::string& name : script.direct_action.names) {
      names.insert(name);
    }
  }

  return base::android::ToJavaArrayOfStrings(
      env, std::vector<std::string>(names.begin(), names.end()));
}

base::android::ScopedJavaLocalRef<jobject>
ClientAndroid::ToJavaAutofillAssistantDirectAction(
    JNIEnv* env,
    const DirectAction& direct_action) const {
  std::set<std::string> names;
  for (const std::string& name : direct_action.names)
    names.insert(name);
  auto jnames = base::android::ToJavaArrayOfStrings(
      env, std::vector<std::string>(names.begin(), names.end()));

  std::vector<std::string> required_arguments;
  for (const std::string& arg : direct_action.required_arguments)
    required_arguments.emplace_back(arg);
  auto jrequired_arguments =
      base::android::ToJavaArrayOfStrings(env, required_arguments);

  std::vector<std::string> optional_arguments;
  for (const std::string& arg : direct_action.optional_arguments)
    optional_arguments.emplace_back(arg);
  auto joptional_arguments =
      base::android::ToJavaArrayOfStrings(env, std::move(optional_arguments));

  return Java_AutofillAssistantDirectActionImpl_Constructor(
      env, jnames, jrequired_arguments, joptional_arguments);
}

base::android::ScopedJavaLocalRef<jobjectArray> ClientAndroid::GetDirectActions(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  DCHECK(controller_ != nullptr);
  // Prepare the java array to hold the direct actions.
  base::android::ScopedJavaLocalRef<jclass> directaction_array_class =
      base::android::GetClass(env,
                              "org/chromium/components/autofill_assistant/"
                              "AutofillAssistantDirectActionImpl",
                              "autofill_assistant");

  const std::vector<ScriptHandle>& direct_action_scripts =
      controller_->GetDirectActionScripts();

  jobjectArray joa = env->NewObjectArray(
      direct_action_scripts.size(), directaction_array_class.obj(), nullptr);
  jni_generator::CheckException(env);

  for (size_t i = 0; i < direct_action_scripts.size(); i++) {
    auto jdirect_action = ToJavaAutofillAssistantDirectAction(
        env, direct_action_scripts.at(i).direct_action);
    env->SetObjectArrayElement(joa, i, jdirect_action.obj());
  }
  return base::android::ScopedJavaLocalRef<jobjectArray>(env, joa);
}

void ClientAndroid::OnFetchWebsiteActions(
    const base::android::JavaRef<jobject>& jcallback) {
  JNIEnv* env = AttachCurrentThread();
  Java_AutofillAssistantClient_onFetchWebsiteActions(
      env, java_object_, jcallback, controller_ != nullptr);
}

bool ClientAndroid::PerformDirectAction(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jstring>& jaction_name,
    const base::android::JavaParamRef<jstring>& jexperiment_ids,
    const base::android::JavaParamRef<jobjectArray>& jparameter_names,
    const base::android::JavaParamRef<jobjectArray>& jparameter_values,
    const base::android::JavaParamRef<jobject>& joverlay_coordinator) {
  std::string action_name =
      base::android::ConvertJavaStringToUTF8(env, jaction_name);

  auto trigger_context = ui_controller_android_utils::CreateTriggerContext(
      env, GetWebContents(), jexperiment_ids, jparameter_names,
      jparameter_values, /* jdevice_only_parameter_names= */
      base::android::JavaParamRef<jobjectArray>(nullptr),
      /* jdevice_only_parameter_values= */
      base::android::JavaParamRef<jobjectArray>(nullptr),
      /* onboarding_shown = */ false,
      /* is_direct_action = */ true,
      /* jinitial_url = */
      nullptr,
      /* is_custom_tab = */
      dependencies_->GetPlatformDependencies()->IsCustomTab(*GetWebContents()));

  int action_index = FindDirectAction(action_name);
  if (action_index == -1)
    return false;

  // If an overlay is already shown, then show the rest of the UI immediately.
  if (joverlay_coordinator) {
    AttachUI(joverlay_coordinator);
  }

  return controller_->PerformDirectAction(action_index,
                                          std::move(trigger_context));
}

void ClientAndroid::ShowFatalError(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  if (!controller_) {
    return;
  }
  controller_->RequireUI();
  controller_->OnFatalError(
      GetDisplayStringUTF8(ClientSettingsProto::DEFAULT_ERROR,
                           controller_->GetSettings()),
      Metrics::DropOutReason::NO_SCRIPTS);
}

bool ClientAndroid::IsSupervisedUser(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  return dependencies_->GetCommonDependencies()->IsSupervisedUser(
      GetWebContents()->GetBrowserContext());
}

void ClientAndroid::OnSpokenFeedbackAccessibilityServiceChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jboolean enabled) {
  if (!ui_controller_) {
    return;
  }
  ui_controller_->OnSpokenFeedbackAccessibilityServiceChanged(enabled);
}

std::string ClientAndroid::GetDebugContext() {
  if (!controller_) {
    return std::string();
  }
  base::Value controller_context = controller_->GetDebugContext();
  if (ui_controller_) {
    base::Value ui_controller_context = ui_controller_->GetDebugContext();
    controller_context.MergeDictionary(&ui_controller_context);
  }
  std::string output_js;
  base::JSONWriter::Write(controller_context, &output_js);
  return output_js;
}

base::android::ScopedJavaGlobalRef<jobject> ClientAndroid::GetDependencies(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  return jdependencies_;
}

int ClientAndroid::FindDirectAction(const std::string& action_name) {
  // It's too late to create a controller. This should have been done in
  // FetchWebsiteActions.
  if (!controller_)
    return -1;

  const std::vector<ScriptHandle>& direct_action_scripts =
      controller_->GetDirectActionScripts();
  for (size_t i = 0; i < direct_action_scripts.size(); i++) {
    const base::flat_set<std::string>& action_names =
        direct_action_scripts.at(i).direct_action.names;
    if (action_names.count(action_name) != 0)
      return i;
  }

  return -1;
}

void ClientAndroid::AttachUI() {
  AttachUI(nullptr);
}

void ClientAndroid::AttachUI(
    const base::android::JavaRef<jobject>& joverlay_coordinator) {
  if (!ui_controller_android_) {
    ui_controller_android_ = UiControllerAndroid::CreateFromWebContents(
        GetWebContents(), jdependencies_, joverlay_coordinator);
    if (!ui_controller_android_) {
      // The activity is not or not yet in a mode where attaching the UI is
      // possible.
      return;
    }
  }
  has_had_ui_ = true;

  if (!ui_controller_android_->IsAttached() ||
      (controller_ != nullptr &&
       !ui_controller_android_->IsAttachedTo(controller_.get()))) {
    if (!controller_)
      CreateController(/* service= */ nullptr,
                       /* trigger_script= */ absl::nullopt);
    ui_controller_android_->Attach(GetWebContents(), this, controller_.get(),
                                   ui_controller_.get());
  }
}

void ClientAndroid::DestroyUISoon() {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&ClientAndroid::DestroyUI,
                                weak_ptr_factory_.GetWeakPtr()));
}

void ClientAndroid::DestroyUI() {
  ui_controller_android_.reset();
}

version_info::Channel ClientAndroid::GetChannel() const {
  return dependencies_->GetCommonDependencies()->GetChannel();
}

std::string ClientAndroid::GetEmailAddressForAccessTokenAccount() const {
  JNIEnv* env = AttachCurrentThread();
  return base::android::ConvertJavaStringToUTF8(
      Java_AutofillAssistantClient_getEmailAddressForAccessTokenAccount(
          env, java_object_));
}

std::string ClientAndroid::GetSignedInEmail() const {
  return dependencies_->GetCommonDependencies()->GetSignedInEmail(
      GetWebContents()->GetBrowserContext());
}

absl::optional<std::pair<int, int>> ClientAndroid::GetWindowSize() const {
  if (ui_controller_android_) {
    return ui_controller_android_->GetWindowSize();
  }
  return absl::nullopt;
}

ClientContextProto::ScreenOrientation ClientAndroid::GetScreenOrientation()
    const {
  if (ui_controller_android_) {
    return ui_controller_android_->GetScreenOrientation();
  }
  return ClientContextProto::UNDEFINED_ORIENTATION;
}

void ClientAndroid::FetchPaymentsClientToken(
    base::OnceCallback<void(const std::string&)> callback) {
  DCHECK(!fetch_payments_client_token_callback_);
  fetch_payments_client_token_callback_ = std::move(callback);

  Java_AutofillAssistantClient_fetchPaymentsClientToken(AttachCurrentThread(),
                                                        java_object_);
}

void ClientAndroid::OnPaymentsClientToken(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    const JavaParamRef<jbyteArray>& jclient_token) {
  if (!fetch_payments_client_token_callback_) {
    return;
  }
  std::string client_token;
  base::android::JavaByteArrayToString(AttachCurrentThread(), jclient_token,
                                       &client_token);
  std::move(fetch_payments_client_token_callback_).Run(client_token);
}

AccessTokenFetcher* ClientAndroid::GetAccessTokenFetcher() {
  return this;
}

autofill::PersonalDataManager* ClientAndroid::GetPersonalDataManager() const {
  return dependencies_->GetCommonDependencies()->GetPersonalDataManager(
      GetWebContents()->GetBrowserContext());
}

WebsiteLoginManager* ClientAndroid::GetWebsiteLoginManager() const {
  if (!website_login_manager_) {
    auto* password_manager_client =
        dependencies_->GetCommonDependencies()->GetPasswordManagerClient(
            GetWebContents());
    if (password_manager_client) {
      website_login_manager_ = std::make_unique<WebsiteLoginManagerImpl>(
          password_manager_client, GetWebContents());
    } else {
      website_login_manager_ = std::make_unique<EmptyWebsiteLoginManagerImpl>();
    }
  }
  return website_login_manager_.get();
}

password_manager::PasswordChangeSuccessTracker*
ClientAndroid::GetPasswordChangeSuccessTracker() const {
  return password_manager::PasswordChangeSuccessTrackerFactory::
      GetForBrowserContext(GetWebContents()->GetBrowserContext());
}

std::string ClientAndroid::GetLocale() const {
  // TODO(b/201964911): use dependencies instead.
  return base::android::GetDefaultLocaleString();
}

std::string ClientAndroid::GetCountryCode() const {
  return dependencies_->GetCommonDependencies()->GetCountryCode();
}

DeviceContext ClientAndroid::GetDeviceContext() const {
  DeviceContext context;
  Version version;
  version.sdk_int = Java_AutofillAssistantClient_getSdkInt(
      AttachCurrentThread(), java_object_);

  context.version = version;
  context.manufacturer = base::android::ConvertJavaStringToUTF8(
      Java_AutofillAssistantClient_getDeviceManufacturer(AttachCurrentThread(),
                                                         java_object_));
  context.model = base::android::ConvertJavaStringToUTF8(
      Java_AutofillAssistantClient_getDeviceModel(AttachCurrentThread(),
                                                  java_object_));
  return context;
}

bool ClientAndroid::IsAccessibilityEnabled() const {
  return dependencies_->IsAccessibilityEnabled();
}

bool ClientAndroid::IsSpokenFeedbackAccessibilityServiceEnabled() const {
  return Java_AutofillAssistantClient_isSpokenFeedbackAccessibilityServiceEnabled(
      AttachCurrentThread(), java_object_);
}

content::WebContents* ClientAndroid::GetWebContents() const {
  // While a const_cast is not ideal. The Autofill API uses const in various
  // spots and the content public API doesn't have const accessors. So the const
  // cast is the lesser of two evils.
  return const_cast<content::WebContents*>(
      &content::WebContentsUserData<ClientAndroid>::GetWebContents());
}

void ClientAndroid::RecordDropOut(Metrics::DropOutReason reason) {
  if (started_)
    Metrics::RecordDropOut(reason, intent_);

  started_ = false;
}

bool ClientAndroid::HasHadUI() const {
  return has_had_ui_;
}

ScriptExecutorUiDelegate* ClientAndroid::GetScriptExecutorUiDelegate() {
  return ui_controller_.get();
}

bool ClientAndroid::MustUseBackendData() const {
  // For WebLayer flows the client does not have access to Chrome's Autofill
  // data and must use data from our backend. Similarly the client can not use
  // e.g. Autofill's data editors and must rely on GMS Core provided
  // replacements.
  return dependencies_->GetCommonDependencies()->IsWebLayer();
}

void ClientAndroid::GetAnnotateDomModelVersion(
    base::OnceCallback<void(absl::optional<int64_t>)> callback) const {
  if (!annotate_dom_model_service_) {
    std::move(callback).Run(absl::nullopt);
    return;
  }

  auto model_version = annotate_dom_model_service_->GetModelVersion();
  if (model_version.has_value()) {
    std::move(callback).Run(model_version);
    return;
  }

  annotate_dom_model_service_->NotifyOnModelFileAvailable(
      base::BindOnce(&ClientAndroid::OnAnnotateDomModelFileAvailable,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

bool ClientAndroid::IsXmlSigned(const std::string& xml_string) const {
  JNIEnv* env = AttachCurrentThread();
  jboolean j_output = Java_AssistantParseSingleTagXmlUtilWrapper_isXmlSigned(
      env, ConvertUTF8ToJavaString(env, xml_string));

  return (j_output == JNI_TRUE);
}

const std::vector<std::string> ClientAndroid::ExtractValuesFromSingleTagXml(
    const std::string& xml_string,
    const std::vector<std::string>& keys) const {
  JNIEnv* env = AttachCurrentThread();
  auto j_output_values =
      Java_AssistantParseSingleTagXmlUtilWrapper_extractValuesFromSingleTagXml(
          env, ConvertUTF8ToJavaString(env, xml_string),
          ToJavaArrayOfStrings(env, std::move(keys)));

  std::vector<std::string> output_values;
  AppendJavaStringArrayToStringVector(env, j_output_values, &output_values);
  return output_values;
}

void ClientAndroid::OnAnnotateDomModelFileAvailable(
    base::OnceCallback<void(absl::optional<int64_t>)> callback,
    bool available) {
  DCHECK(annotate_dom_model_service_);
  std::move(callback).Run(annotate_dom_model_service_->GetModelVersion());
}

void ClientAndroid::Shutdown(Metrics::DropOutReason reason) {
  if (!controller_)
    return;

  // Shutdown in a separate task. This avoids tricky ordering issues when
  // Shutdown is called from the controller or the ui_controller.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&ClientAndroid::SafeDestroyControllerAndUI,
                                weak_ptr_factory_.GetWeakPtr(), reason));
}

void ClientAndroid::SafeDestroyControllerAndUI(Metrics::DropOutReason reason) {
  if (started_) {
    Metrics::RecordDropOut(reason, intent_);
  }

  DestroyUI();
  DestroyController();
}

void ClientAndroid::FetchAccessToken(
    base::OnceCallback<void(bool, const std::string&)> callback) {
  DCHECK(!fetch_access_token_callback_);

  fetch_access_token_callback_ = std::move(callback);
  JNIEnv* env = AttachCurrentThread();
  Java_AutofillAssistantClient_fetchAccessToken(env, java_object_);
}

void ClientAndroid::InvalidateAccessToken(const std::string& access_token) {
  JNIEnv* env = AttachCurrentThread();
  Java_AutofillAssistantClient_invalidateAccessToken(
      env, java_object_,
      base::android::ConvertUTF8ToJavaString(env, access_token));
}

void ClientAndroid::CreateController(
    std::unique_ptr<Service> service,
    const absl::optional<TriggerScriptProto>& trigger_script) {
  // Persist status message and progress bar when transitioning from trigger
  // script.
  std::string status_message;
  absl::optional<ShowProgressBarProto::StepProgressBarConfiguration>
      progress_bar_config;
  absl::optional<int> progress_bar_active_step;
  if (trigger_script.has_value()) {
    status_message = trigger_script->user_interface()
                         .regular_script_loading_status_message();
    if (trigger_script->user_interface().has_progress_bar()) {
      progress_bar_config =
          ShowProgressBarProto::StepProgressBarConfiguration();
      for (const auto& step_icon :
           trigger_script->user_interface().progress_bar().step_icons()) {
        *progress_bar_config->add_annotated_step_icons()->mutable_icon() =
            step_icon;
      }
      progress_bar_active_step =
          trigger_script->user_interface().progress_bar().active_step();
    }
  }

  DestroyController();
  std::unique_ptr<AutofillAssistantTtsController> tts_controller =
      ui_controller_android_utils::GetTtsControllerToInject(
          AttachCurrentThread());
  if (!tts_controller) {
    tts_controller = std::make_unique<AutofillAssistantTtsController>(
        content::TtsController::GetInstance());
  }
  controller_ = std::make_unique<Controller>(
      GetWebContents(), /* client= */ this,
      base::DefaultTickClock::GetInstance(),
      RuntimeManager::GetForWebContents(GetWebContents())->GetWeakPtr(),
      std::move(service), /* web_controller= */ nullptr,
      ukm::UkmRecorder::Get(), annotate_dom_model_service_);
  ui_controller_ = std::make_unique<UiController>(
      /* client= */ this, controller_.get(), std::move(tts_controller));
  ui_controller_->StartListening();
  ui_controller_->SetStatusMessage(status_message);
  if (progress_bar_config) {
    ui_controller_->SetStepProgressBarConfiguration(*progress_bar_config);
    ui_controller_->SetProgressActiveStep(*progress_bar_active_step);
  }
}

void ClientAndroid::DestroyController() {
  if (controller_ && ui_controller_android_ &&
      ui_controller_android_->IsAttachedTo(controller_.get())) {
    ui_controller_android_->Detach();
  }
  ui_controller_.reset();
  controller_.reset();
  started_ = false;
}

bool ClientAndroid::NeedsUI() {
  return !ui_controller_android_ && controller_ && controller_->NeedsUI();
}

bool ClientAndroid::GetMakeSearchesAndBrowsingBetterEnabled() const {
  return dependencies_->GetCommonDependencies()
      ->GetMakeSearchesAndBrowsingBetterEnabled(
          GetWebContents()->GetBrowserContext());
}

bool ClientAndroid::GetMetricsReportingEnabled() const {
  return dependencies_->GetCommonDependencies()->GetMetricsReportingEnabled(
      GetWebContents()->GetBrowserContext());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ClientAndroid);

}  // namespace autofill_assistant
