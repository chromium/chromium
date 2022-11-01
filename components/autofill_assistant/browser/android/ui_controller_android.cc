// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/android/ui_controller_android.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/locale_utils.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/field_trial_params.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill_assistant/android/jni_headers/AssistantCollectUserDataModel_jni.h"
#include "components/autofill_assistant/android/jni_headers/AssistantDetailsModel_jni.h"
#include "components/autofill_assistant/android/jni_headers/AssistantDetails_jni.h"
#include "components/autofill_assistant/android/jni_headers/AssistantFormInput_jni.h"
#include "components/autofill_assistant/android/jni_headers/AssistantFormModel_jni.h"
#include "components/autofill_assistant/android/jni_headers/AssistantGenericUiModel_jni.h"
#include "components/autofill_assistant/android/jni_headers/AssistantInfoBoxModel_jni.h"
#include "components/autofill_assistant/android/jni_headers/AssistantInfoBox_jni.h"
#include "components/autofill_assistant/android/jni_headers/AssistantLegalDisclaimerModel_jni.h"
#include "components/autofill_assistant/android/jni_headers/AssistantModel_jni.h"
#include "components/autofill_assistant/android/jni_headers/AssistantOverlayModel_jni.h"
#include "components/autofill_assistant/android/jni_headers/AssistantPlaceholdersConfiguration_jni.h"
#include "components/autofill_assistant/android/jni_headers/AssistantQrCodeUtil_jni.h"
#include "components/autofill_assistant/android/jni_headers/AutofillAssistantUiController_jni.h"
#include "components/autofill_assistant/browser/android/client_android.h"
#include "components/autofill_assistant/browser/android/dependencies_android.h"
#include "components/autofill_assistant/browser/android/generic_ui_root_controller_android.h"
#include "components/autofill_assistant/browser/android/ui_controller_android_utils.h"
#include "components/autofill_assistant/browser/bottom_sheet_state.h"
#include "components/autofill_assistant/browser/client_settings.h"
#include "components/autofill_assistant/browser/controller.h"
#include "components/autofill_assistant/browser/display_strings_util.h"
#include "components/autofill_assistant/browser/event_handler.h"
#include "components/autofill_assistant/browser/features.h"
#include "components/autofill_assistant/browser/metrics.h"
#include "components/autofill_assistant/browser/public/rectf.h"
#include "components/autofill_assistant/browser/user_data.h"
#include "components/autofill_assistant/browser/user_data_util.h"
#include "components/autofill_assistant/browser/user_model.h"
#include "components/strings/grit/components_strings.h"
#include "components/version_info/channel.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/google_api_keys.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using ::autofill_assistant::ui_controller_android_utils::
    ConvertNativeOptionalStringToJava;
using ::autofill_assistant::ui_controller_android_utils::CreateJavaInfoPopup;
using ::base::android::AttachCurrentThread;
using ::base::android::ConvertUTF16ToJavaString;
using ::base::android::ConvertUTF8ToJavaString;
using ::base::android::JavaParamRef;
using ::base::android::JavaRef;
using ::base::android::ScopedJavaGlobalRef;
using ::base::android::ScopedJavaLocalRef;
using ::base::android::ToJavaArrayOfStrings;
using ::base::android::ToJavaByteArray;

namespace autofill_assistant {

namespace {

std::vector<float> ToFloatVector(const std::vector<RectF>& areas) {
  std::vector<float> flattened;
  for (const auto& rect : areas) {
    flattened.emplace_back(rect.left);
    flattened.emplace_back(rect.top);
    flattened.emplace_back(rect.right);
    flattened.emplace_back(rect.bottom);
    flattened.emplace_back(rect.full_width ? 1 : 0);
  }
  return flattened;
}

ScopedJavaLocalRef<jobject> CreateOptionalJavaInfoPopup(
    JNIEnv* env,
    const LoginChoice& login_choice,
    const ClientSettings& client_settings,
    const base::android::ScopedJavaGlobalRef<jobject> jinfo_page_util) {
  ScopedJavaLocalRef<jobject> jinfo_popup = nullptr;
  if (login_choice.info_popup.has_value()) {
    jinfo_popup = CreateJavaInfoPopup(
        env, *login_choice.info_popup, jinfo_page_util,
        GetDisplayStringUTF8(ClientSettingsProto::CLOSE, client_settings),
        /* jdelegate= */ nullptr);
  }
  return jinfo_popup;
}

ScopedJavaLocalRef<jobject> CreateJavaLoginChoice(
    JNIEnv* env,
    const LoginChoice& login_choice,
    const ClientSettings& client_settings,
    const base::android::ScopedJavaGlobalRef<jobject> jinfo_page_util) {
  return Java_AssistantCollectUserDataModel_createLoginChoice(
      env, ConvertUTF8ToJavaString(env, login_choice.identifier),
      ConvertUTF8ToJavaString(env, login_choice.label),
      ConvertUTF8ToJavaString(env, login_choice.sublabel),
      ConvertNativeOptionalStringToJava(
          env, login_choice.sublabel_accessibility_hint),
      login_choice.preselect_priority,
      CreateOptionalJavaInfoPopup(env, login_choice, client_settings,
                                  jinfo_page_util),
      ConvertNativeOptionalStringToJava(
          env, login_choice.edit_button_content_description));
}

// Creates the Java equivalent to |login_choices|.
ScopedJavaLocalRef<jobject> CreateJavaLoginChoiceList(
    JNIEnv* env,
    const std::vector<LoginChoice>& login_choices,
    const ClientSettings& client_settings,
    const base::android::ScopedJavaGlobalRef<jobject> jinfo_page_util) {
  auto jlist = Java_AssistantCollectUserDataModel_createLoginChoiceList(env);
  for (const auto& login_choice : login_choices) {
    Java_AssistantCollectUserDataModel_addLoginChoice(
        env, jlist, ConvertUTF8ToJavaString(env, login_choice.identifier),
        ConvertUTF8ToJavaString(env, login_choice.label),
        ConvertUTF8ToJavaString(env, login_choice.sublabel),
        ConvertNativeOptionalStringToJava(
            env, login_choice.sublabel_accessibility_hint),
        login_choice.preselect_priority,
        CreateOptionalJavaInfoPopup(env, login_choice, client_settings,
                                    jinfo_page_util),
        ConvertNativeOptionalStringToJava(
            env, login_choice.edit_button_content_description));
  }
  return jlist;
}

// Creates the java equivalent to the text inputs specified in |section|.
base::android::ScopedJavaLocalRef<jobject> CreateJavaTextInputsForSection(
    JNIEnv* env,
    const TextInputSectionProto& section) {
  auto jinput_list =
      Java_AssistantCollectUserDataModel_createTextInputList(env);
  for (const auto& input : section.input_fields()) {
    TextInputType type;
    switch (input.input_type()) {
      case TextInputProto::INPUT_TEXT:
        type = TextInputType::INPUT_TEXT;
        break;
      case TextInputProto::INPUT_ALPHANUMERIC:
        type = TextInputType::INPUT_ALPHANUMERIC;
        break;
      case TextInputProto::UNDEFINED:
        NOTREACHED();
        continue;
    }
    Java_AssistantCollectUserDataModel_appendTextInput(
        env, jinput_list, type, ConvertUTF8ToJavaString(env, input.hint()),
        ConvertUTF8ToJavaString(env, input.value()),
        ConvertUTF8ToJavaString(env, input.client_memory_key()));
  }
  return jinput_list;
}

absl::optional<int> GetPreviousFormCounterResult(
    const FormProto::Result* result,
    int input_index,
    int counter_index) {
  if (result == nullptr) {
    return absl::nullopt;
  }

  if (input_index >= result->input_results().size()) {
    return absl::nullopt;
  }
  auto input_result = result->input_results(input_index);

  if (counter_index >= input_result.counter().values().size()) {
    return absl::nullopt;
  }
  return input_result.counter().values(counter_index);
}

absl::optional<bool> GetPreviousFormSelectionResult(
    const FormProto::Result* result,
    int input_index,
    int selection_index) {
  if (result == nullptr) {
    return absl::nullopt;
  }

  if (input_index >= result->input_results().size()) {
    return absl::nullopt;
  }
  auto input_result = result->input_results(input_index);

  if (selection_index >= input_result.selection().selected().size()) {
    return absl::nullopt;
  }
  return input_result.selection().selected(selection_index);
}

/*
 * Sets the QR Code delegate object and the UI strings for java side
 * AssistantQrCodeImagePickerModelWrapper. It then triggers java util function
 * to prompt QR Code Scanning via Image Picker.
 */
void PromptQrCodeImagePicker(
    JNIEnv* env,
    base::android::ScopedJavaGlobalRef<jobject>
        java_ui_controller_android_object,
    const PromptQrCodeScanProto_ImagePickerUiStrings* image_picker_ui_strings,
    const AssistantQrCodeImagePickerModelWrapper&
        qr_code_image_picker_model_wrapper,
    base::android::ScopedJavaGlobalRef<jobject> java_qr_code_native_delegate) {
  // Register delegate for the QR Code Image Picker UI
  qr_code_image_picker_model_wrapper.SetDelegate(java_qr_code_native_delegate);

  // Set UI strings in model
  qr_code_image_picker_model_wrapper.SetToolbarTitle(
      image_picker_ui_strings->title_text());
  qr_code_image_picker_model_wrapper.SetPermissionText(
      image_picker_ui_strings->permission_text());
  qr_code_image_picker_model_wrapper.SetPermissionButtonText(
      image_picker_ui_strings->permission_button_text());
  qr_code_image_picker_model_wrapper.SetOpenSettingsText(
      image_picker_ui_strings->open_settings_text());
  qr_code_image_picker_model_wrapper.SetOpenSettingsButtonText(
      image_picker_ui_strings->open_settings_button_text());

  Java_AssistantQrCodeUtil_promptQrCodeImagePicker(
      env,
      Java_AutofillAssistantUiController_getDependencies(
          env, java_ui_controller_android_object),
      qr_code_image_picker_model_wrapper.GetModel());
}

/*
 * Sets the QR Code delegate object and the UI strings for java side
 * AssistantQrCodeCameraScanModelWrapper. It then triggers java util function
 * to prompt QR Code Scanning via Camera Preview.
 */
void PromptQrCodeCameraScan(
    JNIEnv* env,
    base::android::ScopedJavaGlobalRef<jobject>
        java_ui_controller_android_object,
    const PromptQrCodeScanProto_CameraScanUiStrings* camera_scan_ui_strings,
    const AssistantQrCodeCameraScanModelWrapper&
        qr_code_camera_scan_model_wrapper,
    base::android::ScopedJavaGlobalRef<jobject> java_qr_code_native_delegate) {
  // Register delegate for the QR Code Camera Scan UI
  qr_code_camera_scan_model_wrapper.SetDelegate(java_qr_code_native_delegate);

  // Set UI strings in model
  qr_code_camera_scan_model_wrapper.SetToolbarTitle(
      camera_scan_ui_strings->title_text());
  qr_code_camera_scan_model_wrapper.SetPermissionText(
      camera_scan_ui_strings->permission_text());
  qr_code_camera_scan_model_wrapper.SetPermissionButtonText(
      camera_scan_ui_strings->permission_button_text());
  qr_code_camera_scan_model_wrapper.SetOpenSettingsText(
      camera_scan_ui_strings->open_settings_text());
  qr_code_camera_scan_model_wrapper.SetOpenSettingsButtonText(
      camera_scan_ui_strings->open_settings_button_text());
  qr_code_camera_scan_model_wrapper.SetCameraPreviewInstructionText(
      camera_scan_ui_strings->camera_preview_instruction_text());
  qr_code_camera_scan_model_wrapper.SetCameraPreviewSecurityText(
      camera_scan_ui_strings->camera_preview_security_text());

  Java_AssistantQrCodeUtil_promptQrCodeCameraScan(
      env,
      Java_AutofillAssistantUiController_getDependencies(
          env, java_ui_controller_android_object),
      qr_code_camera_scan_model_wrapper.GetModel());
}

// Analog to
// PersonalDataManagerAndroid::GetShippingAddressLabelForPaymentRequest.
base::android::ScopedJavaLocalRef<jstring> GetShippingAddressLabel(
    JNIEnv* env,
    const autofill::AutofillProfile* profile,
    bool with_country) {
  if (!profile) {
    return ConvertUTF8ToJavaString(env, std::string());
  }

  // The full name is not included in the label for shipping address. It is
  // added separately instead.
  static constexpr autofill::ServerFieldType kLabelFields[] = {
      autofill::ServerFieldType::COMPANY_NAME,
      autofill::ServerFieldType::ADDRESS_HOME_LINE1,
      autofill::ServerFieldType::ADDRESS_HOME_LINE2,
      autofill::ServerFieldType::ADDRESS_HOME_DEPENDENT_LOCALITY,
      autofill::ServerFieldType::ADDRESS_HOME_CITY,
      autofill::ServerFieldType::ADDRESS_HOME_STATE,
      autofill::ServerFieldType::ADDRESS_HOME_ZIP,
      autofill::ServerFieldType::ADDRESS_HOME_SORTING_CODE,
      autofill::ServerFieldType::ADDRESS_HOME_COUNTRY,
  };
  size_t fields_size = std::size(kLabelFields);

  // For the case where country is omitted, do not use the last field.
  if (!with_country) {
    --fields_size;
  }

  return ConvertUTF16ToJavaString(
      env, profile->ConstructInferredLabel(
               kLabelFields, /* label_fields_size= */ fields_size,
               /* num_fields_to_include= */ fields_size,
               base::android::GetDefaultLocaleString()));
}

}  // namespace

// static
std::unique_ptr<UiControllerAndroid> UiControllerAndroid::CreateFromWebContents(
    content::WebContents* web_contents,
    const base::android::JavaRef<jobject>& jdependencies,
    const base::android::JavaRef<jobject>& joverlay_coordinator) {
  JNIEnv* env = AttachCurrentThread();
  if (!Java_AutofillAssistantUiController_canAttachUi(
          env, web_contents->GetJavaWebContents(), jdependencies)) {
    return nullptr;
  }

  return std::make_unique<UiControllerAndroid>(env, web_contents, jdependencies,
                                               joverlay_coordinator);
}

UiControllerAndroid::UiControllerAndroid(
    JNIEnv* env,
    content::WebContents* web_contents,
    const base::android::JavaRef<jobject>& jdependencies,
    const base::android::JavaRef<jobject>& joverlay_coordinator)
    : overlay_delegate_(this),
      header_delegate_(this),
      collect_user_data_delegate_(this),
      form_delegate_(this),
      generic_ui_delegate_(this),
      bottom_bar_delegate_(this),
      dependencies_(
          DependenciesAndroid::CreateFromJavaDependencies(jdependencies)) {
  java_object_ = Java_AutofillAssistantUiController_Constructor(
      env, reinterpret_cast<intptr_t>(this), web_contents->GetJavaWebContents(),
      jdependencies,
      /* allowTabSwitching= */
      base::FeatureList::IsEnabled(features::kAutofillAssistantChromeEntry),
      joverlay_coordinator);
  header_model_ = std::make_unique<AssistantHeaderModel>(
      Java_AssistantModel_getHeaderModel(env, GetModel()));

  // Register overlay_delegate_ as delegate for the overlay.
  Java_AssistantOverlayModel_setDelegate(env, GetOverlayModel(),
                                         overlay_delegate_.GetJavaObject());

  // Register header_delegate_ as delegate for clicks on header buttons.
  header_model_->SetDelegate(header_delegate_);

  // Register collect_user_data_delegate_ as delegate for the collect user
  // data UI.
  Java_AssistantCollectUserDataModel_setDelegate(
      env, GetCollectUserDataModel(),
      collect_user_data_delegate_.GetJavaObject());
  Java_AssistantModel_setBottomBarDelegate(
      env, GetModel(), bottom_bar_delegate_.GetJavaObject());
}

void UiControllerAndroid::Attach(content::WebContents* web_contents,
                                 ClientAndroid* client,
                                 ExecutionDelegate* execution_delegate,
                                 UiDelegate* ui_delegate) {
  DCHECK(web_contents);
  DCHECK(client);
  DCHECK(ui_delegate);
  DCHECK(execution_delegate);

  client_ = client;

  // Detach from the current execution_delegate and ui_delegate, if previously
  // set.
  Detach();

  // Attach to the new ui_delegate.
  ui_delegate_ = ui_delegate;
  ui_delegate_->AddObserver(this);

  // Attach to the new execution_delegate.
  execution_delegate_ = execution_delegate;
  execution_delegate_->AddObserver(this);

  JNIEnv* env = AttachCurrentThread();
  auto java_web_contents = web_contents->GetJavaWebContents();
  Java_AssistantCollectUserDataModel_setWebContents(
      env, GetCollectUserDataModel(), java_web_contents);
  Java_AssistantOverlayModel_setWebContents(env, GetOverlayModel(),
                                            java_web_contents);

  OnClientSettingsChanged(execution_delegate_->GetClientSettings());
  Java_AssistantModel_setPeekModeDisabled(env, GetModel(), false);

  if (execution_delegate_->GetState() != AutofillAssistantState::INACTIVE &&
      execution_delegate_->IsTabSelected()) {
    // The UI was created for an existing Controller.
    RestoreUi();
  } else if (execution_delegate_->GetState() ==
             AutofillAssistantState::INACTIVE) {
    SetVisible(true);
  }
  // The call to set the web contents will, for some edge cases, trigger a call
  // from the Java side to the onTabSelected method.
  // We want this to happen only after the AttachUI method was fully executed,
  // as it would otherwise find that IsTabSelected() is true when deciding if
  // restoring the UI.
  Java_AssistantModel_setWebContents(env, GetModel(), java_web_contents);
}

void UiControllerAndroid::Detach() {
  if (ui_delegate_) {
    ui_delegate_->RemoveObserver(this);
  }
  if (execution_delegate_) {
    execution_delegate_->RemoveObserver(this);
  }
  ui_delegate_ = nullptr;
  execution_delegate_ = nullptr;
}

UiControllerAndroid::~UiControllerAndroid() {
  Java_AutofillAssistantUiController_clearNativePtr(AttachCurrentThread(),
                                                    java_object_);
  if (execution_delegate_) {
    execution_delegate_->SetUiShown(false);
  }
  Detach();
}

base::android::ScopedJavaLocalRef<jobject> UiControllerAndroid::GetModel() {
  return Java_AutofillAssistantUiController_getModel(AttachCurrentThread(),
                                                     java_object_);
}

void UiControllerAndroid::OnStateChanged(AutofillAssistantState new_state) {
  if (!Java_AssistantModel_getVisible(AttachCurrentThread(), GetModel())) {
    // Leave the UI alone as long as it's invisible. Missed state changes will
    // be recovered by SetVisible(true).
    return;
  }
  SetupForState();
}

void UiControllerAndroid::SetupForState() {
  DCHECK(execution_delegate_ != nullptr);
  DCHECK(ui_delegate_ != nullptr);

  UpdateActions(ui_delegate_->GetUserActions());
  AutofillAssistantState state = execution_delegate_->GetState();
  Java_AssistantModel_setHandleBackPress(
      AttachCurrentThread(), GetModel(),
      state != AutofillAssistantState::BROWSE);

  bool should_prompt_action_expand_sheet =
      ui_delegate_->ShouldPromptActionExpandSheet();
  switch (state) {
    case AutofillAssistantState::STARTING:
      SetOverlayState(OverlayState::FULL);
      SetSpinPoodle(true);
      return;

    case AutofillAssistantState::RUNNING:
      SetOverlayState(OverlayState::FULL);
      SetSpinPoodle(true);
      return;

    case AutofillAssistantState::PROMPT:
      SetOverlayState(OverlayState::PARTIAL);
      SetSpinPoodle(false);

      if (should_prompt_action_expand_sheet &&
          execution_delegate_->IsTabSelected())
        ShowContentAndExpandBottomSheet();
      return;

    case AutofillAssistantState::BROWSE:
      SetOverlayState(OverlayState::HIDDEN);
      SetSpinPoodle(false);
      return;

    case AutofillAssistantState::MODAL_DIALOG:
      SetOverlayState(OverlayState::FULL);
      SetSpinPoodle(true);
      return;

    case AutofillAssistantState::STOPPED:
      SetOverlayState(OverlayState::HIDDEN);
      SetSpinPoodle(false);

      // Make sure the user sees the error message.
      if (execution_delegate_->IsTabSelected())
        ShowContentAndExpandBottomSheet();
      ResetGenericUiControllers();
      return;

    case AutofillAssistantState::TRACKING:
      SetOverlayState(OverlayState::HIDDEN);
      SetSpinPoodle(false);

      if (!execution_delegate_->NeedsUI()) {
        Java_AssistantModel_setVisible(AttachCurrentThread(), GetModel(),
                                       false);
        DestroySelf();
      } else if (execution_delegate_->IsTabSelected()) {
        ShowContentAndExpandBottomSheet();
      }
      return;

    case AutofillAssistantState::INACTIVE:
      // Wait for the state to change.
      return;
  }
  NOTREACHED() << "Unknown state: " << static_cast<int>(state);
}

void UiControllerAndroid::OnKeyboardSuppressionStateChanged(
    bool should_suppress_keyboard) {
  Java_AssistantModel_setAllowSoftKeyboard(AttachCurrentThread(), GetModel(),
                                           !should_suppress_keyboard);
}

void UiControllerAndroid::OnStatusMessageChanged(const std::string& message) {
  header_model_->SetStatusMessage(message);
}

void UiControllerAndroid::OnBubbleMessageChanged(const std::string& message) {
  if (!message.empty()) {
    header_model_->SetBubbleMessage(message);
  }
}

void UiControllerAndroid::OnProgressActiveStepChanged(int active_step) {
  header_model_->SetProgressActiveStep(active_step);
}

void UiControllerAndroid::OnProgressVisibilityChanged(bool visible) {
  header_model_->SetProgressVisible(visible);
}

void UiControllerAndroid::OnProgressBarErrorStateChanged(bool error) {
  header_model_->SetProgressBarErrorState(error);
}

void UiControllerAndroid::OnStepProgressBarConfigurationChanged(
    const ShowProgressBarProto::StepProgressBarConfiguration& configuration) {
  header_model_->SetStepProgressBarConfiguration(
      configuration,
      Java_AutofillAssistantUiController_getContext(AttachCurrentThread(),
                                                    java_object_),
      *dependencies_);
}

void UiControllerAndroid::OnViewportModeChanged(ViewportMode mode) {
  Java_AutofillAssistantUiController_setViewportMode(AttachCurrentThread(),
                                                     java_object_, mode);
}

void UiControllerAndroid::OnPeekModeChanged(
    ConfigureBottomSheetProto::PeekMode peek_mode) {
  Java_AutofillAssistantUiController_setPeekMode(AttachCurrentThread(),
                                                 java_object_, peek_mode);
}

void UiControllerAndroid::OnExpandBottomSheet() {
  Java_AutofillAssistantUiController_expandBottomSheet(AttachCurrentThread(),
                                                       java_object_);
}

void UiControllerAndroid::OnCollapseBottomSheet() {
  Java_AutofillAssistantUiController_collapseBottomSheet(AttachCurrentThread(),
                                                         java_object_);
}

void UiControllerAndroid::OnOverlayColorsChanged(
    const ExecutionDelegate::OverlayColors& colors) {
  JNIEnv* env = AttachCurrentThread();
  auto overlay_model = GetOverlayModel();
  Java_AssistantOverlayModel_setBackgroundColor(
      env, overlay_model,
      ui_controller_android_utils::GetJavaColor(env, colors.background));
  Java_AssistantOverlayModel_setHighlightBorderColor(
      env, overlay_model,
      ui_controller_android_utils::GetJavaColor(env, colors.highlight_border));
}

void UiControllerAndroid::ShowContentAndExpandBottomSheet() {
  Java_AutofillAssistantUiController_showContentAndExpandBottomSheet(
      AttachCurrentThread(), java_object_);
}

void UiControllerAndroid::SetSpinPoodle(bool enabled) {
  header_model_->SetSpinPoodle(enabled);
}

void UiControllerAndroid::OnHeaderFeedbackButtonClicked() {
  JNIEnv* env = AttachCurrentThread();
  // If the feedback is sent by interacting with the header, it's more likely
  // that there is a problem with the bottomsheet, so in this case we don't send
  // the website's screenshot (COMPOSITOR).
  Java_AutofillAssistantUiController_showFeedback(
      env, java_object_,
      ConvertUTF8ToJavaString(env, client_->GetDebugContext()),
      /* screenshotMode */ 0);
}

void UiControllerAndroid::OnQrCodeScanFinished(
    const ClientStatus& status,
    const absl::optional<ValueProto>& value) {
  ui_delegate_->OnQrCodeScanFinished(status, value);
}

void UiControllerAndroid::OnViewEvent(const EventHandler::EventKey& key) {
  ui_delegate_->DispatchEvent(key);
}

void UiControllerAndroid::OnValueChanged(const std::string& identifier,
                                         const ValueProto& value) {
  execution_delegate_->GetUserModel()->SetValue(identifier, value);
}

void UiControllerAndroid::Shutdown(Metrics::DropOutReason reason) {
  client_->Shutdown(reason);
}

void UiControllerAndroid::ShowSnackbar(base::TimeDelta delay,
                                       const std::string& message,
                                       const std::string& undo_string,
                                       base::OnceCallback<void()> action) {
  if (delay.is_zero()) {
    std::move(action).Run();
    return;
  }

  JNIEnv* env = AttachCurrentThread();
  auto jmodel = GetModel();
  if (!Java_AssistantModel_getVisible(env, jmodel)) {
    // If the UI is not visible, execute the action immediately.
    std::move(action).Run();
    return;
  }
  SetVisible(false);
  snackbar_action_ = std::move(action);
  Java_AutofillAssistantUiController_showSnackbar(
      env, java_object_, static_cast<jint>(delay.InMilliseconds()),
      ConvertUTF8ToJavaString(env, message),
      ConvertUTF8ToJavaString(env, undo_string));
}

void UiControllerAndroid::SnackbarResult(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jboolean undo) {
  base::OnceCallback<void()> action = std::move(snackbar_action_);
  if (!action) {
    NOTREACHED();
    return;
  }
  if (undo) {
    SetVisible(true);
    return;
  }
  std::move(action).Run();
}

void UiControllerAndroid::DestroySelf() {
  // Note: shutdown happens asynchronously (if at all), so calling code after
  // |ShutdownIfNecessary| returns should be ok.
  if (execution_delegate_)
    execution_delegate_->ShutdownIfNecessary();

  // Destroy self in separate task to avoid UaFs. Obviously, having this method
  // in the first place is a terrible idea. We should refactor. The controller
  // should always be in control. ui_controller should always hold a reference
  // to ui_delegate, even when detached. ui_controller should not have a
  // reference for client_android at all. It introduces a circular dependency,
  // among other things.
  //
  // TODO(mcarlen): refactor lifecycle and deps of ui_controller.
  client_->DestroyUISoon();
}

void UiControllerAndroid::SetVisible(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jboolean visible) {
  SetVisible(visible);
}

void UiControllerAndroid::SetVisible(bool visible) {
  Java_AssistantModel_setVisible(AttachCurrentThread(), GetModel(), visible);
  DCHECK(execution_delegate_);
  if (visible) {
    // Recover possibly state changes missed by OnStateChanged()
    SetupForState();
  } else {
    SetOverlayState(OverlayState::HIDDEN);
  }
  bool should_suppress_keyboard =
      visible && execution_delegate_->ShouldSuppressKeyboard();
  execution_delegate_->SuppressKeyboard(should_suppress_keyboard);
  OnKeyboardSuppressionStateChanged(should_suppress_keyboard);
  execution_delegate_->SetUiShown(visible);
}

void UiControllerAndroid::RestoreUi() {
  if (!execution_delegate_ || !ui_delegate_)
    return;

  OnStatusMessageChanged(ui_delegate_->GetStatusMessage());
  OnBubbleMessageChanged(ui_delegate_->GetBubbleMessage());
  OnClientSettingsDisplayStringsChanged(
      execution_delegate_->GetClientSettings());
  OnStepProgressBarConfigurationChanged(
      ui_delegate_->GetStepProgressBarConfiguration());
  OnProgressActiveStepChanged(ui_delegate_->GetProgressActiveStep());
  OnProgressBarErrorStateChanged(ui_delegate_->GetProgressBarErrorState());
  OnProgressVisibilityChanged(ui_delegate_->GetProgressVisible());
  OnInfoBoxChanged(ui_delegate_->GetInfoBox());
  OnDetailsChanged(ui_delegate_->GetDetails());
  OnUserActionsChanged(ui_delegate_->GetUserActions());
  OnCollectUserDataOptionsChanged(ui_delegate_->GetCollectUserDataOptions());
  OnCollectUserDataUiStateChanged(/* loading= */ false,
                                  UserDataEventField::NONE);
  OnUserDataChanged(*execution_delegate_->GetUserData(),
                    UserDataFieldChange::ALL);
  OnPersistentGenericUserInterfaceChanged(
      ui_delegate_->GetPersistentGenericUiProto());
  OnGenericUserInterfaceChanged(ui_delegate_->GetGenericUiProto());
  OnQrCodeScanUiChanged(ui_delegate_->GetPromptQrCodeScanProto());

  std::vector<RectF> area;
  execution_delegate_->GetTouchableArea(&area);
  std::vector<RectF> restricted_area;
  execution_delegate_->GetRestrictedArea(&restricted_area);
  RectF visual_viewport;
  execution_delegate_->GetVisualViewport(&visual_viewport);
  OnTouchableAreaChanged(visual_viewport, area, restricted_area);
  OnViewportModeChanged(execution_delegate_->GetViewportMode());
  OnPeekModeChanged(ui_delegate_->GetPeekMode());
  OnFormChanged(ui_delegate_->GetForm(), ui_delegate_->GetFormResult());
  OnLegalDisclaimerChanged(ui_delegate_->GetLegalDisclaimer());

  ExecutionDelegate::OverlayColors colors;
  execution_delegate_->GetOverlayColors(&colors);
  OnOverlayColorsChanged(colors);
  OnTtsButtonVisibilityChanged(ui_delegate_->GetTtsButtonVisible());
  OnTtsButtonStateChanged(ui_delegate_->GetTtsButtonState());
  OnDisableScrollbarFadingChanged(ui_delegate_->GetDisableScrollbarFading());
  SetVisible(true);
  Java_AutofillAssistantUiController_restoreBottomSheetState(
      AttachCurrentThread(), java_object_,
      ui_controller_android_utils::ToJavaBottomSheetState(
          ui_delegate_->GetBottomSheetState()));
}

void UiControllerAndroid::OnTabSwitched(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jint state,
    jboolean activity_changed) {
  if (!execution_delegate_ || !ui_delegate_)
    return;

  ui_delegate_->SetBottomSheetState(
      ui_controller_android_utils::ToNativeBottomSheetState(state));
  execution_delegate_->SetTabSelected(false);
}

void UiControllerAndroid::OnTabSelected(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  if (execution_delegate_ == nullptr)
    return;

  if (!execution_delegate_->IsTabSelected()) {
    RestoreUi();
    execution_delegate_->SetTabSelected(true);
  }
}

// Actions carousels related methods.

void UiControllerAndroid::UpdateActions(
    const std::vector<UserAction>& user_actions) {
  DCHECK(execution_delegate_);
  DCHECK(ui_delegate_);
  JNIEnv* env = AttachCurrentThread();

  bool has_close_or_cancel = false;
  auto jchips = Java_AutofillAssistantUiController_createChipList(env);
  auto jsticky_chips = Java_AutofillAssistantUiController_createChipList(env);
  int user_action_count = static_cast<int>(user_actions.size());
  for (int i = 0; i < user_action_count; i++) {
    const auto& action = user_actions[i];
    const Chip& chip = action.chip();
    base::android::ScopedJavaLocalRef<jobject> jchip;
    // TODO(arbesser): Refactor this to use
    // ui_controller_android_utils::CreateJavaAssistantChip.
    switch (chip.type) {
      default:  // Ignore actions with other chip types or with no chips.
        break;

      case HIGHLIGHTED_ACTION:
        jchip =
            Java_AutofillAssistantUiController_createHighlightedActionButton(
                env, java_object_, static_cast<int>(chip.type), chip.icon,
                ConvertUTF8ToJavaString(env, chip.text), i, !action.enabled(),
                chip.sticky, chip.visible,
                chip.is_content_description_set
                    ? ConvertUTF8ToJavaString(env, chip.content_description)
                    : nullptr);
        break;

      case NORMAL_ACTION:
        jchip = Java_AutofillAssistantUiController_createActionButton(
            env, java_object_, static_cast<int>(chip.type), chip.icon,
            ConvertUTF8ToJavaString(env, chip.text), i, !action.enabled(),
            chip.sticky, chip.visible,
            chip.is_content_description_set
                ? ConvertUTF8ToJavaString(env, chip.content_description)
                : nullptr);
        break;

      case FEEDBACK_ACTION:
        // A "Send feedback" button which will show the feedback form before
        // executing the action.
        jchip = Java_AutofillAssistantUiController_createFeedbackButton(
            env, java_object_, static_cast<int>(chip.type), chip.icon,
            ConvertUTF8ToJavaString(env, chip.text), i, !action.enabled(),
            chip.sticky, chip.visible,
            chip.is_content_description_set
                ? ConvertUTF8ToJavaString(env, chip.content_description)
                : nullptr);
        break;

      case CANCEL_ACTION:
        // A Cancel button sneaks in an UNDO snackbar before executing the
        // action, while a close button behaves like a normal button.
        jchip = Java_AutofillAssistantUiController_createCancelButton(
            env, java_object_, static_cast<int>(chip.type), chip.icon,
            ConvertUTF8ToJavaString(env, chip.text), i, !action.enabled(),
            chip.sticky, chip.visible,
            chip.is_content_description_set
                ? ConvertUTF8ToJavaString(env, chip.content_description)
                : nullptr);
        has_close_or_cancel = true;
        break;

      case CLOSE_ACTION:
        jchip = Java_AutofillAssistantUiController_createActionButton(
            env, java_object_, static_cast<int>(chip.type), chip.icon,
            ConvertUTF8ToJavaString(env, chip.text), i, !action.enabled(),
            chip.sticky, chip.visible,
            chip.is_content_description_set
                ? ConvertUTF8ToJavaString(env, chip.content_description)
                : nullptr);
        has_close_or_cancel = true;
        break;

      case DONE_ACTION:
        jchip =
            Java_AutofillAssistantUiController_createHighlightedActionButton(
                env, java_object_, static_cast<int>(chip.type), chip.icon,
                ConvertUTF8ToJavaString(env, chip.text), i, !action.enabled(),
                chip.sticky, chip.visible,
                chip.is_content_description_set
                    ? ConvertUTF8ToJavaString(env, chip.content_description)
                    : nullptr);
        has_close_or_cancel = true;
        break;
    }
    if (jchip) {
      Java_AutofillAssistantUiController_appendChipToList(env, jchips, jchip);
      if (chip.sticky) {
        Java_AutofillAssistantUiController_appendChipToList(env, jsticky_chips,
                                                            jchip);
      }
    }
  }

  // Special case: don't create a default cancel chip during shutdown.
  if (!has_close_or_cancel && !ui_delegate_->IsUiShuttingDown()) {
    base::android::ScopedJavaLocalRef<jobject> jcancel_chip;
    if (execution_delegate_->GetState() == AutofillAssistantState::STOPPED ||
        execution_delegate_->GetState() == AutofillAssistantState::TRACKING) {
      jcancel_chip = Java_AutofillAssistantUiController_createCloseButton(
          env, java_object_, static_cast<int>(CLOSE_ACTION), ICON_CLEAR,
          ConvertUTF8ToJavaString(env, ""),
          /* disabled= */ false, /* sticky= */ true, /* visible=*/true,
          /* contentDescription= */ nullptr);
    } else if (execution_delegate_->GetState() !=
               AutofillAssistantState::INACTIVE) {
      jcancel_chip = Java_AutofillAssistantUiController_createCancelButton(
          env, java_object_, static_cast<int>(CANCEL_ACTION), ICON_CLEAR,
          ConvertUTF8ToJavaString(env, ""), -1,
          /* disabled= */ false, /* sticky= */ true, /* visible=*/true,
          /* contentDescription= */ nullptr);
    }

    if (jcancel_chip) {
      Java_AutofillAssistantUiController_appendChipToList(env, jchips,
                                                          jcancel_chip);
      Java_AutofillAssistantUiController_appendChipToList(env, jsticky_chips,
                                                          jcancel_chip);
    }
  }

  Java_AutofillAssistantUiController_setActions(env, java_object_, jchips);
  header_model_->SetChips(jsticky_chips);
}

void UiControllerAndroid::OnUserActionsChanged(
    const std::vector<UserAction>& actions) {
  UpdateActions(actions);
}

void UiControllerAndroid::OnUserActionSelected(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jint index) {
  if (ui_delegate_)
    ui_delegate_->PerformUserAction(index);
}

void UiControllerAndroid::OnCancelButtonClicked(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jint index) {
  // If the keyboard is currently shown, clicking the cancel button should
  // hide the keyboard rather than close autofill assistant, because the cancel
  // chip will be displayed right above the keyboard.
  if (Java_AutofillAssistantUiController_isKeyboardShown(env, java_object_)) {
    Java_AutofillAssistantUiController_hideKeyboard(env, java_object_);
    return;
  }

  CloseOrCancel(index, Metrics::DropOutReason::SHEET_CLOSED);
}

void UiControllerAndroid::OnCloseButtonClicked(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  DestroySelf();
}

void UiControllerAndroid::OnFeedbackButtonClicked(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jint index) {
  // Show the feedback form then directly run the associated action.
  // Unfortunately there is no way to associate a callback to run after the user
  // actually sent (or close) the form, so we have to continue directly after
  // showing it. It should be good enough, given that in most use cases we will
  // directly stop.
  Java_AutofillAssistantUiController_showFeedback(
      env, java_object_,
      ConvertUTF8ToJavaString(env, client_->GetDebugContext()),
      /* screenshotMode */ 1);

  OnUserActionSelected(env, jcaller, index);
}

void UiControllerAndroid::OnTtsButtonClicked() {
  ui_delegate_->OnTtsButtonClicked();
}

void UiControllerAndroid::OnKeyboardVisibilityChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jboolean visible) {
  if (ui_delegate_)
    ui_delegate_->OnKeyboardVisibilityChanged(visible);
}

bool UiControllerAndroid::OnBackButtonClicked() {
  // If the keyboard is currently shown, clicking the back button should
  // hide the keyboard rather than close autofill assistant.
  if (Java_AutofillAssistantUiController_isKeyboardShown(AttachCurrentThread(),
                                                         java_object_)) {
    Java_AutofillAssistantUiController_hideKeyboard(AttachCurrentThread(),
                                                    java_object_);
    return true;
  }

  // For BROWSE state the back button should react in its default way.
  if (execution_delegate_ != nullptr &&
      (execution_delegate_->GetState() == AutofillAssistantState::BROWSE)) {
    return false;
  }

  if (execution_delegate_ == nullptr ||
      execution_delegate_->GetState() == AutofillAssistantState::STOPPED) {
    if (client_->GetWebContents() != nullptr &&
        client_->GetWebContents()->GetController().CanGoBack()) {
      client_->GetWebContents()->GetController().GoBack();
    }

    return true;
  }

  // execution_delegate_ must never be nullptr here!
  CloseOrCancel(-1, Metrics::DropOutReason::BACK_BUTTON_CLICKED);
  return true;
}

void UiControllerAndroid::OnBottomSheetClosedWithSwipe() {
  // Nothing to do
}

void UiControllerAndroid::CloseOrCancel(int action_index,
                                        Metrics::DropOutReason dropout_reason) {
  // Close immediately.
  if (!execution_delegate_ ||
      execution_delegate_->GetState() == AutofillAssistantState::STOPPED) {
    DestroySelf();
    return;
  }

  // Close, with an action.
  const std::vector<UserAction>& user_actions = ui_delegate_->GetUserActions();
  if (action_index >= 0 &&
      static_cast<size_t>(action_index) < user_actions.size() &&
      user_actions[action_index].chip().type == CLOSE_ACTION &&
      ui_delegate_->PerformUserAction(action_index)) {
    return;
  }

  // Cancel, with a snackbar to allow UNDO.
  ShowSnackbar(execution_delegate_->GetClientSettings().cancel_delay,
               GetDisplayStringUTF8(ClientSettingsProto::STOPPED,
                                    execution_delegate_->GetClientSettings()),
               GetDisplayStringUTF8(ClientSettingsProto::UNDO,
                                    execution_delegate_->GetClientSettings()),
               base::BindOnce(&UiControllerAndroid::OnCancel,
                              weak_ptr_factory_.GetWeakPtr(), action_index,
                              dropout_reason));
}

absl::optional<std::pair<int, int>> UiControllerAndroid::GetWindowSize() const {
  JNIEnv* env = AttachCurrentThread();
  auto java_size_array =
      Java_AutofillAssistantUiController_getWindowSize(env, java_object_);
  if (!java_size_array) {
    return absl::nullopt;
  }

  std::vector<int> size_array;
  base::android::JavaIntArrayToIntVector(env, java_size_array, &size_array);
  DCHECK_EQ(size_array.size(), 2u);
  return std::make_pair(size_array[0], size_array[1]);
}

ClientContextProto::ScreenOrientation
UiControllerAndroid::GetScreenOrientation() const {
  int orientation = Java_AutofillAssistantUiController_getScreenOrientation(
      AttachCurrentThread(), java_object_);
  switch (orientation) {
    case 1:
      return ClientContextProto::PORTRAIT;
    case 2:
      return ClientContextProto::LANDSCAPE;
    default:
      return ClientContextProto::UNDEFINED_ORIENTATION;
  }
}

void UiControllerAndroid::OnCancel(int action_index,
                                   Metrics::DropOutReason dropout_reason) {
  if (action_index == -1 || !ui_delegate_ ||
      !ui_delegate_->PerformUserAction(action_index)) {
    Shutdown(dropout_reason);
  }
}

// Overlay related methods.
base::android::ScopedJavaLocalRef<jobject>
UiControllerAndroid::GetOverlayModel() {
  return Java_AssistantModel_getOverlayModel(AttachCurrentThread(), GetModel());
}

void UiControllerAndroid::SetOverlayState(OverlayState state) {
  desired_overlay_state_ = state;

  // Ensure that we don't set partial state if the touchable area is empty. This
  // is important because a partial overlay will be hidden if TalkBack is
  // enabled, and we want to completely prevent TalkBack from accessing the web
  // page if there is no touchable area.
  if (state == OverlayState::PARTIAL) {
    std::vector<RectF> area;
    execution_delegate_->GetTouchableArea(&area);
    if (area.empty()) {
      state = OverlayState::FULL;
    }
  }
  overlay_state_ = state;

  if (execution_delegate_ && execution_delegate_->ShouldShowOverlay()) {
    ApplyOverlayState(state);
  }
}

void UiControllerAndroid::ApplyOverlayState(OverlayState state) {
  Java_AssistantOverlayModel_setState(AttachCurrentThread(), GetOverlayModel(),
                                      state);
  Java_AssistantModel_setAllowTalkbackOnWebsite(
      AttachCurrentThread(), GetModel(), state != OverlayState::FULL);
}

void UiControllerAndroid::OnShouldShowOverlayChanged(bool should_show) {
  if (should_show) {
    ApplyOverlayState(overlay_state_);
  } else {
    ApplyOverlayState(OverlayState::HIDDEN);
  }
}

void UiControllerAndroid::OnTtsButtonVisibilityChanged(bool visible) {
  header_model_->SetTtsButtonVisible(visible);
}

void UiControllerAndroid::OnTtsButtonStateChanged(TtsButtonState state) {
  header_model_->SetTtsButtonState(state);
}

void UiControllerAndroid::OnDisableScrollbarFadingChanged(
    bool disable_scrollbar_fading) {
  Java_AssistantModel_setDisableScrollbarFading(
      AttachCurrentThread(), GetModel(), disable_scrollbar_fading);
}

void UiControllerAndroid::OnTouchableAreaChanged(
    const RectF& visual_viewport,
    const std::vector<RectF>& touchable_areas,
    const std::vector<RectF>& restricted_areas) {
  if (!touchable_areas.empty() &&
      desired_overlay_state_ == OverlayState::PARTIAL) {
    SetOverlayState(OverlayState::PARTIAL);
  }

  JNIEnv* env = AttachCurrentThread();

  Java_AssistantOverlayModel_setVisualViewport(
      env, GetOverlayModel(), visual_viewport.left, visual_viewport.top,
      visual_viewport.right, visual_viewport.bottom);
  Java_AssistantOverlayModel_setTouchableArea(
      env, GetOverlayModel(),
      base::android::ToJavaFloatArray(env, ToFloatVector(touchable_areas)));
  Java_AssistantOverlayModel_setRestrictedArea(
      AttachCurrentThread(), GetOverlayModel(),
      base::android::ToJavaFloatArray(env, ToFloatVector(restricted_areas)));
}

void UiControllerAndroid::OnUnexpectedTaps() {
  if (!execution_delegate_) {
    Shutdown(Metrics::DropOutReason::OVERLAY_STOP);
    return;
  }

  ShowSnackbar(execution_delegate_->GetClientSettings().tap_shutdown_delay,
               GetDisplayStringUTF8(ClientSettingsProto::MAYBE_GIVE_UP,
                                    execution_delegate_->GetClientSettings()),
               GetDisplayStringUTF8(ClientSettingsProto::UNDO,
                                    execution_delegate_->GetClientSettings()),
               base::BindOnce(&UiControllerAndroid::Shutdown,
                              weak_ptr_factory_.GetWeakPtr(),
                              Metrics::DropOutReason::OVERLAY_STOP));
}

// Other methods.
void UiControllerAndroid::CloseCustomTab() {
  Java_AutofillAssistantUiController_scheduleCloseCustomTab(
      AttachCurrentThread(), java_object_);
}

// Collect user data related methods.

base::android::ScopedJavaLocalRef<jobject>
UiControllerAndroid::GetCollectUserDataModel() {
  return Java_AssistantModel_getCollectUserDataModel(AttachCurrentThread(),
                                                     GetModel());
}

void UiControllerAndroid::OnShippingAddressChanged(
    std::unique_ptr<autofill::AutofillProfile> address,
    UserDataEventType event_type) {
  ui_delegate_->HandleShippingAddressChange(std::move(address), event_type);
}

void UiControllerAndroid::OnContactInfoChanged(
    std::unique_ptr<autofill::AutofillProfile> profile,
    UserDataEventType event_type) {
  ui_delegate_->HandleContactInfoChange(std::move(profile), event_type);
}

void UiControllerAndroid::OnPhoneNumberChanged(
    std::unique_ptr<autofill::AutofillProfile> profile,
    UserDataEventType event_type) {
  ui_delegate_->HandlePhoneNumberChange(std::move(profile), event_type);
}

void UiControllerAndroid::OnCreditCardChanged(
    std::unique_ptr<autofill::CreditCard> card,
    std::unique_ptr<autofill::AutofillProfile> billing_profile,
    UserDataEventType event_type) {
  ui_delegate_->HandleCreditCardChange(std::move(card),
                                       std::move(billing_profile), event_type);
}

void UiControllerAndroid::OnTermsAndConditionsChanged(
    TermsAndConditionsState state) {
  ui_delegate_->SetTermsAndConditions(state);
}

void UiControllerAndroid::OnLoginChoiceChanged(const std::string& identifier) {
  ui_delegate_->SetLoginOption(identifier);
}

void UiControllerAndroid::OnTextLinkClicked(int link) {
  ui_delegate_->OnTextLinkClicked(link);
}

void UiControllerAndroid::OnFormActionLinkClicked(int link) {
  ui_delegate_->OnFormActionLinkClicked(link);
}

void UiControllerAndroid::OnLegalDisclaimerLinkClicked(int link) {
  ui_delegate_->OnLegalDisclaimerLinkClicked(link);
}

void UiControllerAndroid::OnKeyValueChanged(const std::string& key,
                                            const ValueProto& value) {
  ui_delegate_->SetAdditionalValue(key, value);
}

void UiControllerAndroid::OnInputTextFocusChanged(bool is_text_focused) {
  ui_delegate_->OnInputTextFocusChanged(is_text_focused);

  if (is_text_focused)
    return;

  // We set a delay to avoid having the keyboard flickering when the focus goes
  // from one text field to another.
  content::GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&UiControllerAndroid::HideKeyboardIfFocusNotOnText,
                     weak_ptr_factory_.GetWeakPtr()),
      base::Milliseconds(50));
}

void UiControllerAndroid::HideKeyboardIfFocusNotOnText() {
  Java_AutofillAssistantUiController_hideKeyboardIfFocusNotOnText(
      AttachCurrentThread(), java_object_);
}

ScopedJavaGlobalRef<jobject> UiControllerAndroid::GetInfoPageUtil() const {
  return dependencies_->CreateInfoPageUtil();
}

void UiControllerAndroid::OnCollectUserDataOptionsChanged(
    const CollectUserDataOptions* collect_user_data_options) {
  JNIEnv* env = AttachCurrentThread();
  auto jmodel = GetCollectUserDataModel();
  if (!collect_user_data_options) {
    ResetGenericUiControllers();
    Java_AssistantCollectUserDataModel_setVisible(env, jmodel, false);
    return;
  }

  Java_AssistantCollectUserDataModel_setAccountEmail(
      env, jmodel,
      base::android::ConvertUTF8ToJavaString(
          env, client_->GetEmailAddressForAccessTokenAccount()));

  Java_AssistantCollectUserDataModel_setShouldStoreUserDataChanges(
      env, jmodel, collect_user_data_options->should_store_data_changes);
  Java_AssistantCollectUserDataModel_setUseAlternativeEditDialogs(
      env, jmodel, collect_user_data_options->use_alternative_edit_dialogs);
  Java_AssistantCollectUserDataModel_setRequestName(
      env, jmodel, collect_user_data_options->request_payer_name);
  Java_AssistantCollectUserDataModel_setRequestEmail(
      env, jmodel, collect_user_data_options->request_payer_email);
  Java_AssistantCollectUserDataModel_setRequestPhone(
      env, jmodel, collect_user_data_options->request_payer_phone);
  std::vector<int> contact_summary_fields;
  for (const auto& field : collect_user_data_options->contact_summary_fields) {
    contact_summary_fields.emplace_back((int)field);
  }
  Java_AssistantCollectUserDataModel_setContactSummaryDescriptionOptions(
      env, jmodel,
      Java_AssistantCollectUserDataModel_createContactDescriptionOptions(
          env, base::android::ToJavaIntArray(env, contact_summary_fields),
          collect_user_data_options->contact_summary_max_lines));
  std::vector<int> contact_full_fields;
  for (const auto& field : collect_user_data_options->contact_full_fields) {
    contact_full_fields.emplace_back((int)field);
  }
  Java_AssistantCollectUserDataModel_setContactFullDescriptionOptions(
      env, jmodel,
      Java_AssistantCollectUserDataModel_createContactDescriptionOptions(
          env, base::android::ToJavaIntArray(env, contact_full_fields),
          collect_user_data_options->contact_full_max_lines));
  Java_AssistantCollectUserDataModel_setRequestPhoneNumberSeparately(
      env, jmodel, collect_user_data_options->request_phone_number_separately);
  Java_AssistantCollectUserDataModel_setRequestShippingAddress(
      env, jmodel, collect_user_data_options->request_shipping);
  Java_AssistantCollectUserDataModel_setRequestPayment(
      env, jmodel, collect_user_data_options->request_payment_method);
  Java_AssistantCollectUserDataModel_setRequestLoginChoice(
      env, jmodel, collect_user_data_options->request_login_choice);
  Java_AssistantCollectUserDataModel_setLoginSectionTitle(
      env, jmodel,
      ConvertUTF8ToJavaString(env,
                              collect_user_data_options->login_section_title));
  Java_AssistantCollectUserDataModel_setContactSectionTitle(
      env, jmodel,
      ConvertUTF8ToJavaString(
          env, collect_user_data_options->contact_details_section_title));
  Java_AssistantCollectUserDataModel_setPhoneNumberSectionTitle(
      env, jmodel,
      ConvertUTF8ToJavaString(
          env, collect_user_data_options->phone_number_section_title));
  Java_AssistantCollectUserDataModel_setShippingSectionTitle(
      env, jmodel,
      ConvertUTF8ToJavaString(
          env, collect_user_data_options->shipping_address_section_title));
  Java_AssistantCollectUserDataModel_setAcceptTermsAndConditionsText(
      env, jmodel,
      ConvertUTF8ToJavaString(
          env, collect_user_data_options->accept_terms_and_conditions_text));
  Java_AssistantCollectUserDataModel_setShowTermsAsCheckbox(
      env, jmodel, collect_user_data_options->show_terms_as_checkbox);
  Java_AssistantCollectUserDataModel_setSupportedBasicCardNetworks(
      env, jmodel,
      base::android::ToJavaArrayOfStrings(
          env, collect_user_data_options->supported_basic_card_networks));
  if (collect_user_data_options->data_origin_notice) {
    const auto& configuration = *collect_user_data_options->data_origin_notice;
    Java_AssistantCollectUserDataModel_setDataOriginNoticeConfiguration(
        env, jmodel, ConvertUTF8ToJavaString(env, configuration.link_text()),
        ConvertUTF8ToJavaString(env, configuration.dialog_title()),
        ConvertUTF8ToJavaString(env, configuration.dialog_text()),
        ConvertUTF8ToJavaString(env, configuration.dialog_button_text()));
  }
  if (collect_user_data_options->request_login_choice) {
    auto jlist = CreateJavaLoginChoiceList(
        env, collect_user_data_options->login_choices,
        execution_delegate_->GetClientSettings(), GetInfoPageUtil());
    Java_AssistantCollectUserDataModel_setLoginChoices(env, jmodel, jlist);
  }
  Java_AssistantCollectUserDataModel_setTermsRequireReviewText(
      env, jmodel,
      ConvertUTF8ToJavaString(
          env, collect_user_data_options->terms_require_review_text));
  Java_AssistantCollectUserDataModel_setInfoSectionText(
      env, jmodel,
      ConvertUTF8ToJavaString(env,
                              collect_user_data_options->info_section_text),
      collect_user_data_options->info_section_text_center);
  Java_AssistantCollectUserDataModel_setPrivacyNoticeText(
      env, jmodel,
      ConvertUTF8ToJavaString(env,
                              collect_user_data_options->privacy_notice_text));

  Java_AssistantCollectUserDataModel_setPrependedSections(
      env, jmodel,
      CreateJavaAdditionalSections(
          env, collect_user_data_options->additional_prepended_sections));
  Java_AssistantCollectUserDataModel_setAppendedSections(
      env, jmodel,
      CreateJavaAdditionalSections(
          env, collect_user_data_options->additional_appended_sections));

  if (collect_user_data_options->generic_user_interface_prepended.has_value()) {
    collect_user_data_prepended_generic_ui_controller_ =
        CreateGenericUiControllerForProto(
            *collect_user_data_options->generic_user_interface_prepended);
    Java_AssistantCollectUserDataModel_setGenericUserInterfacePrepended(
        env, jmodel,
        collect_user_data_prepended_generic_ui_controller_ != nullptr
            ? collect_user_data_prepended_generic_ui_controller_->GetRootView()
            : nullptr);
  }
  if (collect_user_data_options->generic_user_interface_appended.has_value()) {
    collect_user_data_appended_generic_ui_controller_ =
        CreateGenericUiControllerForProto(
            *collect_user_data_options->generic_user_interface_appended);
    Java_AssistantCollectUserDataModel_setGenericUserInterfaceAppended(
        env, jmodel,
        collect_user_data_appended_generic_ui_controller_ != nullptr
            ? collect_user_data_appended_generic_ui_controller_->GetRootView()
            : nullptr);
  }
  if (collect_user_data_options->add_payment_instrument_action_token
          .has_value()) {
    Java_AssistantCollectUserDataModel_setAddPaymentInstrumentActionToken(
        env, jmodel,
        ToJavaByteArray(
            env,
            *collect_user_data_options->add_payment_instrument_action_token));
  }
  if (collect_user_data_options->add_address_token.has_value()) {
    Java_AssistantCollectUserDataModel_setInitializeAddressCollectionParams(
        env, jmodel,
        ToJavaByteArray(env, *collect_user_data_options->add_address_token));
  }

  Java_AssistantCollectUserDataModel_setVisible(env, jmodel, true);
}

void UiControllerAndroid::OnCollectUserDataUiStateChanged(
    bool loading,
    UserDataEventField event_field) {
  JNIEnv* env = AttachCurrentThread();
  auto jmodel = GetCollectUserDataModel();

  Java_AssistantCollectUserDataModel_setEnableUiInteractions(env, jmodel,
                                                             !loading);
  Java_AssistantCollectUserDataModel_setMarkContactsLoading(
      env, jmodel, loading && event_field == UserDataEventField::CONTACT_EVENT);
  Java_AssistantCollectUserDataModel_setMarkPhoneNumbersLoading(
      env, jmodel,
      loading && event_field == UserDataEventField::PHONE_NUMBER_EVENT);
  Java_AssistantCollectUserDataModel_setMarkShippingAddressesLoading(
      env, jmodel,
      loading && event_field == UserDataEventField::SHIPPING_EVENT);
  Java_AssistantCollectUserDataModel_setMarkPaymentMethodsLoading(
      env, jmodel,
      loading && event_field == UserDataEventField::CREDIT_CARD_EVENT);
}

void UiControllerAndroid::OnUserDataChanged(const UserData& user_data,
                                            UserDataFieldChange field_change) {
  DCHECK(execution_delegate_ != nullptr);
  DCHECK(ui_delegate_ != nullptr);
  const CollectUserDataOptions* collect_user_data_options =
      ui_delegate_->GetCollectUserDataOptions();
  if (collect_user_data_options == nullptr) {
    // If there are no options, there currently is no active
    // CollectUserDataAction, the UI is not shown and does not need to be
    // updated.
    return;
  }

  JNIEnv* env = AttachCurrentThread();
  auto jmodel = GetCollectUserDataModel();

  if (field_change == UserDataFieldChange::ALL ||
      field_change == UserDataFieldChange::TERMS_AND_CONDITIONS) {
    Java_AssistantCollectUserDataModel_setTermsStatus(
        env, jmodel, user_data.terms_and_conditions_);
  }

  if (field_change == UserDataFieldChange::ALL ||
      field_change == UserDataFieldChange::AVAILABLE_PROFILES) {
    // Contacts.
    auto jcontactlist =
        Java_AssistantCollectUserDataModel_createContactList(env);
    auto contact_indices = user_data::SortContactsByCompleteness(
        *collect_user_data_options, user_data.available_contacts_);
    for (int index : contact_indices) {
      const auto& errors = user_data::GetContactValidationErrors(
          user_data.available_contacts_[index]->profile.get(),
          *collect_user_data_options);
      Java_AssistantCollectUserDataModel_addContact(
          env, jcontactlist,
          ui_controller_android_utils::CreateAssistantAutofillProfile(
              env, *user_data.available_contacts_[index]->profile,
              base::android::GetDefaultLocaleString()),
          base::android::ToJavaArrayOfStrings(env, errors),
          user_data.available_contacts_[index]->can_edit);
    }
    Java_AssistantCollectUserDataModel_setAvailableContacts(env, jmodel,
                                                            jcontactlist);

    // Phone numbers.
    auto jphone_number_list =
        Java_AssistantCollectUserDataModel_createContactList(env);
    auto phone_number_indices = user_data::SortPhoneNumbersByCompleteness(
        *collect_user_data_options, user_data.available_phone_numbers_);
    for (int index : phone_number_indices) {
      const auto& errors = user_data::GetPhoneNumberValidationErrors(
          user_data.available_phone_numbers_[index]->profile.get(),
          *collect_user_data_options);
      Java_AssistantCollectUserDataModel_addContact(
          env, jphone_number_list,
          ui_controller_android_utils::CreateAssistantAutofillProfile(
              env, *user_data.available_phone_numbers_[index]->profile,
              base::android::GetDefaultLocaleString()),
          base::android::ToJavaArrayOfStrings(env, errors),
          /* canEdit = */ false);
    }
    Java_AssistantCollectUserDataModel_setAvailablePhoneNumbers(
        env, jmodel, jphone_number_list);

    // Billing addresses.
    auto jbillinglist =
        Java_AssistantCollectUserDataModel_createBillingAddressList(env);
    for (const auto& address : user_data.available_addresses_) {
      Java_AssistantCollectUserDataModel_addBillingAddress(
          env, jbillinglist,
          ui_controller_android_utils::CreateAssistantAutofillProfile(
              env, *address->profile, base::android::GetDefaultLocaleString()));
    }
    Java_AssistantCollectUserDataModel_setAvailableBillingAddresses(
        env, jmodel, jbillinglist);

    // Shipping addresses.
    auto jshippinglist =
        Java_AssistantCollectUserDataModel_createShippingAddressList(env);
    auto address_indices = user_data::SortShippingAddressesByCompleteness(
        *collect_user_data_options, user_data.available_addresses_);
    for (int index : address_indices) {
      const auto& shipping_address = user_data.available_addresses_[index];
      const auto& errors = user_data::GetShippingAddressValidationErrors(
          shipping_address->profile.get(), *collect_user_data_options);
      auto jedit_token =
          collect_user_data_options->use_alternative_edit_dialogs
              ? ToJavaByteArray(
                    env, shipping_address->edit_token.value_or(std::string()))
              : nullptr;
      Java_AssistantCollectUserDataModel_addShippingAddress(
          env, jshippinglist,
          ui_controller_android_utils::CreateAssistantAutofillProfile(
              env, *shipping_address->profile,
              base::android::GetDefaultLocaleString()),
          GetShippingAddressLabel(env, shipping_address->profile.get(),
                                  /* with_country= */ true),
          GetShippingAddressLabel(env, shipping_address->profile.get(),
                                  /* with_country= */ false),
          base::android::ToJavaArrayOfStrings(env, errors), jedit_token);
    }
    Java_AssistantCollectUserDataModel_setAvailableShippingAddresses(
        env, jmodel, jshippinglist);
  }

  if (field_change == UserDataFieldChange::ALL ||
      field_change == UserDataFieldChange::AVAILABLE_PROFILES ||
      field_change == UserDataFieldChange::CONTACT_PROFILE) {
    const autofill::AutofillProfile* selected_contact_profile =
        user_data.selected_address(
            collect_user_data_options->contact_details_name);
    auto jselected_contact =
        selected_contact_profile == nullptr
            ? nullptr
            : ui_controller_android_utils::CreateAssistantAutofillProfile(
                  env, *selected_contact_profile,
                  base::android::GetDefaultLocaleString());
    const auto& selected_contact_errors = user_data::GetContactValidationErrors(
        selected_contact_profile, *collect_user_data_options);

    bool can_edit = true;
    if (selected_contact_profile) {
      const auto& contact_it = base::ranges::find_if(
          user_data.available_contacts_,
          [&](const std::unique_ptr<Contact>& contact) {
            return contact->profile &&
                   contact->profile->guid() == selected_contact_profile->guid();
          });
      if (contact_it != user_data.available_contacts_.end()) {
        can_edit = (*contact_it)->can_edit;
      }
    }

    // In the UserDataFieldChange::CONTACT_PROFILE case the selection is
    // already known in Java, but it has no errors. The PDM off case does not
    // set updated contacts.
    Java_AssistantCollectUserDataModel_setSelectedContactDetails(
        env, jmodel, jselected_contact,
        base::android::ToJavaArrayOfStrings(env, selected_contact_errors),
        can_edit);
  }

  if (field_change == UserDataFieldChange::ALL ||
      field_change == UserDataFieldChange::AVAILABLE_PROFILES ||
      field_change == UserDataFieldChange::PHONE_NUMBER) {
    const autofill::AutofillProfile* selected_phone_number =
        user_data.selected_phone_number();
    auto jselected_phone_number =
        selected_phone_number == nullptr
            ? nullptr
            : ui_controller_android_utils::CreateAssistantAutofillProfile(
                  env, *selected_phone_number,
                  base::android::GetDefaultLocaleString());
    const auto& selected_phone_number_errors =
        user_data::GetPhoneNumberValidationErrors(selected_phone_number,
                                                  *collect_user_data_options);

    bool can_edit = true;
    if (selected_phone_number) {
      const auto& phone_number_it = base::ranges::find_if(
          user_data.available_phone_numbers_,
          [&](const std::unique_ptr<PhoneNumber>& phone_number) {
            return phone_number->profile && phone_number->profile->guid() ==
                                                selected_phone_number->guid();
          });
      if (phone_number_it != user_data.available_phone_numbers_.end()) {
        can_edit = (*phone_number_it)->can_edit;
      }
    }

    // In the UserDataFieldChange::PHONE_NUMBER case the selection is already
    // known in Java, but it has no errors. The PDM off case does not set
    // updated phone numbers.
    Java_AssistantCollectUserDataModel_setSelectedPhoneNumber(
        env, jmodel, jselected_phone_number,
        base::android::ToJavaArrayOfStrings(env, selected_phone_number_errors),
        can_edit);
  }

  if (field_change == UserDataFieldChange::ALL ||
      field_change == UserDataFieldChange::AVAILABLE_PROFILES ||
      field_change == UserDataFieldChange::SHIPPING_ADDRESS) {
    const autofill::AutofillProfile* selected_shipping_address =
        user_data.selected_address(
            collect_user_data_options->shipping_address_name);
    auto jselected_shipping_address =
        selected_shipping_address == nullptr
            ? nullptr
            : ui_controller_android_utils::CreateAssistantAutofillProfile(
                  env, *selected_shipping_address,
                  base::android::GetDefaultLocaleString());
    const auto& selected_shipping_address_errors =
        user_data::GetShippingAddressValidationErrors(
            selected_shipping_address, *collect_user_data_options);
    auto jselected_shipping_address_edit_token =
        collect_user_data_options->use_alternative_edit_dialogs
            ? ToJavaByteArray(env, std::string())
            : nullptr;
    if (selected_shipping_address) {
      const auto& address_it = base::ranges::find_if(
          user_data.available_addresses_,
          [&](const std::unique_ptr<Address>& shipping_address) {
            return shipping_address->profile &&
                   shipping_address->profile->guid() ==
                       selected_shipping_address->guid();
          });
      if (address_it != user_data.available_addresses_.end()) {
        const auto& found_shipping_address = *address_it;
        if (found_shipping_address->edit_token.has_value()) {
          jselected_shipping_address_edit_token =
              ToJavaByteArray(env, *found_shipping_address->edit_token);
        }
      }
    }

    // In the UserDataFieldChange::SHIPPING_ADDRESS case the selection is
    // already known in Java, but it has no errors. The PDM off case does not
    // set updated shipping addresses.
    Java_AssistantCollectUserDataModel_setSelectedShippingAddress(
        env, jmodel, jselected_shipping_address,
        GetShippingAddressLabel(env, selected_shipping_address,
                                /* with_country= */ true),
        GetShippingAddressLabel(env, selected_shipping_address,
                                /* with_country= */ false),
        base::android::ToJavaArrayOfStrings(env,
                                            selected_shipping_address_errors),
        jselected_shipping_address_edit_token);
  }

  if (field_change == UserDataFieldChange::ALL ||
      field_change == UserDataFieldChange::AVAILABLE_PAYMENT_INSTRUMENTS) {
    auto jlist =
        Java_AssistantCollectUserDataModel_createAutofillPaymentInstrumentList(
            env);
    auto sorted_payment_instrument_indices =
        user_data::SortPaymentInstrumentsByCompleteness(
            *collect_user_data_options,
            user_data.available_payment_instruments_);
    for (int index : sorted_payment_instrument_indices) {
      const auto& instrument = user_data.available_payment_instruments_[index];
      if (instrument->card == nullptr) {
        NOTREACHED();
        continue;
      }
      const auto& errors = user_data::GetPaymentInstrumentValidationErrors(
          instrument->card.get(), instrument->billing_address.get(),
          *collect_user_data_options);
      auto jedit_token =
          collect_user_data_options->use_alternative_edit_dialogs
              ? ToJavaByteArray(env,
                                instrument->edit_token.value_or(std::string()))
              : nullptr;
      Java_AssistantCollectUserDataModel_addPaymentInstrument(
          env, jlist,
          ui_controller_android_utils::CreateAssistantAutofillCreditCard(
              env, *(instrument->card),
              base::android::GetDefaultLocaleString()),
          instrument->billing_address == nullptr
              ? nullptr
              : ui_controller_android_utils::CreateAssistantAutofillProfile(
                    env, *(instrument->billing_address),
                    base::android::GetDefaultLocaleString()),
          base::android::ToJavaArrayOfStrings(env, errors), jedit_token);
    }
    Java_AssistantCollectUserDataModel_setAvailablePaymentInstruments(
        env, jmodel, jlist);
  }

  if (field_change == UserDataFieldChange::ALL ||
      field_change == UserDataFieldChange::AVAILABLE_PAYMENT_INSTRUMENTS ||
      field_change == UserDataFieldChange::CARD) {
    const autofill::CreditCard* selected_card = user_data.selected_card();
    const autofill::AutofillProfile* selected_billing_address =
        user_data.selected_address(
            collect_user_data_options->billing_address_name);
    auto jselected_card =
        selected_card == nullptr
            ? nullptr
            : ui_controller_android_utils::CreateAssistantAutofillCreditCard(
                  env, *selected_card, base::android::GetDefaultLocaleString());
    auto jselected_billing_address =
        selected_billing_address == nullptr
            ? nullptr
            : ui_controller_android_utils::CreateAssistantAutofillProfile(
                  env, *selected_billing_address,
                  base::android::GetDefaultLocaleString());
    const auto& selected_payment_instrument_errors =
        user_data::GetPaymentInstrumentValidationErrors(
            selected_card, selected_billing_address,
            *collect_user_data_options);
    auto jselected_payment_instrument_edit_token =
        collect_user_data_options->use_alternative_edit_dialogs
            ? ToJavaByteArray(env, std::string())
            : nullptr;
    if (selected_card) {
      const auto& card_it = base::ranges::find_if(
          user_data.available_payment_instruments_,
          [&](const std::unique_ptr<PaymentInstrument>& payment_instrument) {
            return payment_instrument->card &&
                   payment_instrument->card->instrument_id() ==
                       selected_card->instrument_id();
          });
      if (card_it != user_data.available_payment_instruments_.end()) {
        const auto& found_payment_instrument = *card_it;
        if (found_payment_instrument->edit_token.has_value()) {
          jselected_payment_instrument_edit_token =
              ToJavaByteArray(env, *found_payment_instrument->edit_token);
        }
      }
    }

    // Note: Ignore UserDataFieldChange::BILLING_ADDRESS, they are sent in
    // tandem.
    // In the UserDataFieldChange::CARD case the selection is already known
    // in Java, but it has no errors. The PDM off case does not set updated
    // payment instruments.
    Java_AssistantCollectUserDataModel_setSelectedPaymentInstrument(
        env, jmodel, jselected_card, jselected_billing_address,
        base::android::ToJavaArrayOfStrings(env,
                                            selected_payment_instrument_errors),
        jselected_payment_instrument_edit_token);
  }

  if (field_change == UserDataFieldChange::ALL ||
      field_change == UserDataFieldChange::LOGIN_CHOICE) {
    ScopedJavaLocalRef<jobject> jselected_login_choice =
        user_data.selected_login_choice() == nullptr
            ? nullptr
            : CreateJavaLoginChoice(env, *user_data.selected_login_choice(),
                                    execution_delegate_->GetClientSettings(),
                                    GetInfoPageUtil());

    Java_AssistantCollectUserDataModel_setSelectedLoginChoice(
        env, jmodel, jselected_login_choice);
  }
}

// FormProto related methods.
base::android::ScopedJavaLocalRef<jobject> UiControllerAndroid::GetFormModel() {
  return Java_AssistantModel_getFormModel(AttachCurrentThread(), GetModel());
}

base::android::ScopedJavaLocalRef<jobject>
UiControllerAndroid::GetLegalDisclaimerModel() {
  return Java_AssistantModel_getLegalDisclaimerModel(AttachCurrentThread(),
                                                     GetModel());
}

void UiControllerAndroid::OnLegalDisclaimerChanged(
    const LegalDisclaimerProto* legal_disclaimer) {
  JNIEnv* env = AttachCurrentThread();
  auto jmodel = GetLegalDisclaimerModel();

  if (!legal_disclaimer) {
    Java_AssistantLegalDisclaimerModel_setLegalDisclaimer(env, jmodel, nullptr,
                                                          nullptr);
    legal_disclaimer_delegate_.reset();
    return;
  }

  if (!legal_disclaimer_delegate_) {
    legal_disclaimer_delegate_ =
        std::make_unique<AssistantLegalDisclaimerNativeDelegate>(this);
  }

  Java_AssistantLegalDisclaimerModel_setLegalDisclaimer(
      env, jmodel, legal_disclaimer_delegate_->GetJavaObject(),
      ConvertUTF8ToJavaString(env,
                              legal_disclaimer->legal_disclaimer_message()));
}

void UiControllerAndroid::OnFormChanged(const FormProto* form,
                                        const FormProto::Result* result) {
  JNIEnv* env = AttachCurrentThread();

  if (!form) {
    Java_AssistantFormModel_clearInputs(env, GetFormModel());
    return;
  }

  auto jinput_list = Java_AssistantFormModel_createInputList(env);
  for (int i = 0; i < form->inputs_size(); i++) {
    const FormInputProto input = form->inputs(i);

    switch (input.input_type_case()) {
      case FormInputProto::InputTypeCase::kCounter: {
        CounterInputProto counter_input = input.counter();

        auto jcounters = Java_AssistantFormInput_createCounterList(env);
        for (int j = 0; j < counter_input.counters_size(); ++j) {
          const CounterInputProto::Counter& counter = counter_input.counters(j);

          std::vector<int> allowed_values;
          for (int value : counter.allowed_values()) {
            allowed_values.push_back(value);
          }

          auto result_value = GetPreviousFormCounterResult(result, i, j);
          Java_AssistantFormInput_addCounter(
              env, jcounters,
              Java_AssistantFormInput_createCounter(
                  env, ConvertUTF8ToJavaString(env, counter.label()),
                  ConvertUTF8ToJavaString(env, counter.description_line_1()),
                  ConvertUTF8ToJavaString(env, counter.description_line_2()),
                  result_value.has_value() ? result_value.value()
                                           : counter.initial_value(),
                  counter.min_value(), counter.max_value(),
                  base::android::ToJavaIntArray(env, allowed_values)));
        }

        const auto dependencies =
            Java_AutofillAssistantUiController_getDependencies(
                AttachCurrentThread(), java_object_);

        Java_AssistantFormModel_addInput(
            env, jinput_list,
            Java_AssistantFormInput_createCounterInput(
                env, i, ConvertUTF8ToJavaString(env, counter_input.label()),
                ConvertUTF8ToJavaString(env, counter_input.expand_text()),
                ConvertUTF8ToJavaString(env, counter_input.minimize_text()),
                jcounters, dependencies, counter_input.minimized_count(),
                counter_input.min_counters_sum(),
                counter_input.max_counters_sum(),
                form_delegate_.GetJavaObject()));
        break;
      }
      case FormInputProto::InputTypeCase::kSelection: {
        SelectionInputProto selection_input = input.selection();

        auto jchoices = Java_AssistantFormInput_createChoiceList(env);
        for (int j = 0; j < selection_input.choices_size(); ++j) {
          const SelectionInputProto::Choice& choice =
              selection_input.choices(j);

          auto result_value = GetPreviousFormSelectionResult(result, i, j);
          Java_AssistantFormInput_addChoice(
              env, jchoices,
              Java_AssistantFormInput_createChoice(
                  env, ConvertUTF8ToJavaString(env, choice.label()),
                  ConvertUTF8ToJavaString(env, choice.description_line_1()),
                  ConvertUTF8ToJavaString(env, choice.description_line_2()),
                  result_value.has_value() ? result_value.value()
                                           : choice.selected()));
        }

        Java_AssistantFormModel_addInput(
            env, jinput_list,
            Java_AssistantFormInput_createSelectionInput(
                env, i, ConvertUTF8ToJavaString(env, selection_input.label()),
                jchoices, selection_input.allow_multiple(),
                form_delegate_.GetJavaObject()));
        break;
      }
      case FormInputProto::InputTypeCase::INPUT_TYPE_NOT_SET:
        NOTREACHED();
        break;
        // Intentionally no default case to make compilation fail if a new value
        // was added to the enum but not to this list.
    }
  }
  Java_AssistantFormModel_setInputs(env, GetFormModel(), jinput_list);

  if (form->has_info_label()) {
    Java_AssistantFormModel_setInfoLabel(
        env, GetFormModel(), ConvertUTF8ToJavaString(env, form->info_label()));
  } else {
    Java_AssistantFormModel_clearInfoLabel(env, GetFormModel());
  }

  if (form->has_info_popup()) {
    Java_AssistantFormModel_setInfoPopup(
        env, GetFormModel(),
        ui_controller_android_utils::CreateJavaInfoPopup(
            env, form->info_popup(), GetInfoPageUtil(),
            GetDisplayStringUTF8(ClientSettingsProto::CLOSE,
                                 execution_delegate_->GetClientSettings()),
            nullptr));
  } else {
    Java_AssistantFormModel_clearInfoPopup(env, GetFormModel());
  }
}

void UiControllerAndroid::OnClientSettingsDisplayStringsChanged(
    const ClientSettings& settings) {
  header_model_->SetProfileIconMenuSettingsMessage(
      GetDisplayStringUTF8(ClientSettingsProto::SETTINGS, settings));
  header_model_->SetProfileIconMenuSendFeedbackMessage(
      GetDisplayStringUTF8(ClientSettingsProto::SEND_FEEDBACK, settings));
}

void UiControllerAndroid::OnClientSettingsChanged(
    const ClientSettings& settings) {
  JNIEnv* env = AttachCurrentThread();
  Java_AssistantOverlayModel_setTapTracking(
      env, GetOverlayModel(), settings.tap_count,
      settings.tap_tracking_duration.InMilliseconds());

  if (settings.overlay_image.has_value()) {
    auto jcontext =
        Java_AutofillAssistantUiController_getContext(env, java_object_);
    const auto& image = *(settings.overlay_image);
    int image_size = ui_controller_android_utils::GetPixelSizeOrDefault(
        env, jcontext, image.image_size(), 0);
    int top_margin = ui_controller_android_utils::GetPixelSizeOrDefault(
        env, jcontext, image.image_top_margin(), 0);
    int bottom_margin = ui_controller_android_utils::GetPixelSizeOrDefault(
        env, jcontext, image.image_bottom_margin(), 0);
    int text_size = ui_controller_android_utils::GetPixelSizeOrDefault(
        env, jcontext, image.text_size(), 0);

    Java_AssistantOverlayModel_setOverlayImage(
        env, GetOverlayModel(), jcontext,
        ui_controller_android_utils::CreateJavaDrawable(
            env, jcontext, *dependencies_, image.image_drawable(),
            execution_delegate_->GetUserModel()),
        image_size, top_margin, bottom_margin,
        ConvertUTF8ToJavaString(env, image.text()),
        ui_controller_android_utils::GetJavaColor(env, image.text_color()),
        text_size);
  } else {
    Java_AssistantOverlayModel_clearOverlayImage(env, GetOverlayModel());
  }
  if (settings.integration_test_settings.has_value()) {
    header_model_->SetDisableAnimations(
        settings.integration_test_settings->disable_header_animations());
    Java_AutofillAssistantUiController_setDisableChipChangeAnimations(
        env, java_object_,
        settings.integration_test_settings
            ->disable_carousel_change_animations());
  }
  Java_AssistantModel_setTalkbackSheetSizeFraction(
      env, GetModel(), settings.talkback_sheet_size_fraction);
  OnClientSettingsDisplayStringsChanged(settings);
}

void UiControllerAndroid::OnQrCodeScanUiChanged(
    const PromptQrCodeScanProto* qr_code_scan) {
  if (!qr_code_scan) {
    // Action is completed and we will clear all the models and delegate
    // objects. For any new action, we will create it again.
    qr_code_native_delegate_ = nullptr;
    qr_code_camera_scan_model_wrapper_ = nullptr;
    qr_code_image_picker_model_wrapper_ = nullptr;
    return;
  }

  JNIEnv* env = AttachCurrentThread();
  qr_code_native_delegate_ =
      std::make_unique<AssistantQrCodeNativeDelegate>(this);

  base::android::ScopedJavaGlobalRef<jobject> java_qr_code_native_delegate =
      qr_code_native_delegate_->GetJavaObject();

  if (qr_code_scan->use_gallery()) {
    // Create a model to manage state of QR Code Scanning via Image Picker.
    qr_code_image_picker_model_wrapper_ =
        std::make_unique<AssistantQrCodeImagePickerModelWrapper>();

    PromptQrCodeImagePicker(
        env, java_object_, &qr_code_scan->image_picker_ui_strings(),
        *qr_code_image_picker_model_wrapper_, java_qr_code_native_delegate);
  } else {
    // Create a model to manage state of QR Code Scanning via Camera Preview.
    qr_code_camera_scan_model_wrapper_ =
        std::make_unique<AssistantQrCodeCameraScanModelWrapper>();

    PromptQrCodeCameraScan(
        env, java_object_, &qr_code_scan->camera_scan_ui_strings(),
        *qr_code_camera_scan_model_wrapper_, java_qr_code_native_delegate);
  }
}

void UiControllerAndroid::OnGenericUserInterfaceChanged(
    const GenericUserInterfaceProto* generic_ui) {
  // Try to inflate user interface from proto.
  if (generic_ui != nullptr) {
    generic_ui_controller_ = CreateGenericUiControllerForProto(*generic_ui);
    ClientStatus status(generic_ui_controller_ ? ACTION_APPLIED
                                               : INVALID_ACTION);
    ui_delegate_->GetBasicInteractions()->NotifyViewInflationFinished(status);
  } else {
    generic_ui_controller_.reset();
  }

  // Set or clear generic UI.
  Java_AssistantGenericUiModel_setView(
      AttachCurrentThread(), GetGenericUiModel(),
      generic_ui_controller_ != nullptr ? generic_ui_controller_->GetRootView()
                                        : nullptr);
}

void UiControllerAndroid::OnShowAccountScreen(
    const ShowAccountScreenProto& proto,
    const std::string& email_address) {
  JNIEnv* env = AttachCurrentThread();
  Java_AutofillAssistantUiController_showGmsAccountScreenIntent(
      env, java_object_, proto.gms_account_intent_screen_id(),
      base::android::ConvertUTF8ToJavaString(env, email_address));
}

void UiControllerAndroid::OnPersistentGenericUserInterfaceChanged(
    const GenericUserInterfaceProto* generic_ui) {
  // Try to inflate user interface from proto.
  if (generic_ui != nullptr) {
    persistent_generic_ui_controller_ =
        CreateGenericUiControllerForProto(*generic_ui);
    ClientStatus status(persistent_generic_ui_controller_ ? ACTION_APPLIED
                                                          : INVALID_ACTION);

    ui_delegate_->GetBasicInteractions()->NotifyPersistentViewInflationFinished(
        status);
  } else {
    persistent_generic_ui_controller_.reset();
  }

  // Set or clear generic UI.
  Java_AssistantGenericUiModel_setView(
      AttachCurrentThread(), GetPersistentGenericUiModel(),
      persistent_generic_ui_controller_ != nullptr
          ? persistent_generic_ui_controller_->GetRootView()
          : nullptr);
}

void UiControllerAndroid::OnCounterChanged(int input_index,
                                           int counter_index,
                                           int value) {
  ui_delegate_->SetCounterValue(input_index, counter_index, value);
}

void UiControllerAndroid::OnChoiceSelectionChanged(int input_index,
                                                   int choice_index,
                                                   bool selected) {
  ui_delegate_->SetChoiceSelected(input_index, choice_index, selected);
}

// Details related method.

base::android::ScopedJavaLocalRef<jobject>
UiControllerAndroid::GetDetailsModel() {
  return Java_AssistantModel_getDetailsModel(AttachCurrentThread(), GetModel());
}

void UiControllerAndroid::OnDetailsChanged(
    const std::vector<Details>& details_list) {
  JNIEnv* env = AttachCurrentThread();

  auto jdetails_list = Java_AssistantDetailsModel_createDetailsList(env);
  for (const auto& details : details_list) {
    auto opt_image_accessibility_hint = details.imageAccessibilityHint();
    base::android::ScopedJavaLocalRef<jstring> jimage_accessibility_hint =
        nullptr;
    if (opt_image_accessibility_hint.has_value()) {
      jimage_accessibility_hint =
          ConvertUTF8ToJavaString(env, opt_image_accessibility_hint.value());
    }

    // Create the placeholders configuration. We check here that the associated
    // texts/urls are empty, so that on the Java side we can just check the
    // placeholders configuration to know whether a placeholder should be shown
    // or not.
    auto placeholders = details.placeholders();
    auto jplaceholders = Java_AssistantPlaceholdersConfiguration_Constructor(
        env,
        placeholders.show_image_placeholder() && details.imageUrl().empty(),
        placeholders.show_title_placeholder() && details.title().empty(),
        placeholders.show_description_line_1_placeholder() &&
            details.descriptionLine1().empty(),
        placeholders.show_description_line_2_placeholder() &&
            details.descriptionLine2().empty(),
        placeholders.show_description_line_3_placeholder() &&
            details.descriptionLine3().empty());

    auto jdetails = Java_AssistantDetails_create(
        env, ConvertUTF8ToJavaString(env, details.title()),
        ConvertUTF8ToJavaString(env, details.imageUrl()),
        jimage_accessibility_hint, details.imageAllowClickthrough(),
        ConvertUTF8ToJavaString(env, details.imageDescription()),
        ConvertUTF8ToJavaString(env, details.imagePositiveText()),
        ConvertUTF8ToJavaString(env, details.imageNegativeText()),
        ConvertUTF8ToJavaString(env, details.imageClickthroughUrl()),
        ConvertUTF8ToJavaString(env, details.totalPriceLabel()),
        ConvertUTF8ToJavaString(env, details.totalPrice()),
        ConvertUTF8ToJavaString(env, details.descriptionLine1()),
        ConvertUTF8ToJavaString(env, details.descriptionLine2()),
        ConvertUTF8ToJavaString(env, details.descriptionLine3()),
        ConvertUTF8ToJavaString(env, details.priceAttribution()),
        details.userApprovalRequired(), details.highlightTitle(),
        details.highlightLine1(), details.highlightLine2(),
        details.highlightLine3(), jplaceholders);

    Java_AssistantDetailsModel_addDetails(env, jdetails_list, jdetails);
  }

  auto jmodel = GetDetailsModel();
  Java_AssistantDetailsModel_setDetailsList(env, jmodel, jdetails_list);
}

// InfoBox related method.
base::android::ScopedJavaLocalRef<jobject>
UiControllerAndroid::GetInfoBoxModel() {
  return Java_AssistantModel_getInfoBoxModel(AttachCurrentThread(), GetModel());
}

void UiControllerAndroid::OnInfoBoxChanged(const InfoBox* info_box) {
  JNIEnv* env = AttachCurrentThread();
  auto jmodel = GetInfoBoxModel();
  if (!info_box) {
    Java_AssistantInfoBoxModel_clearInfoBox(env, jmodel);
    return;
  }

  const InfoBoxProto& proto = info_box->proto().info_box();
  auto jcontext =
      Java_AutofillAssistantUiController_getContext(env, java_object_);
  absl::optional<DrawableProto> drawable_proto;
  bool use_instrinsic_dimensions = false;
  switch (proto.image_case()) {
    case InfoBoxProto::ImageCase::kImagePath: {
      if (!proto.image_path().empty()) {
        drawable_proto.emplace();
        auto* bitmap_proto = drawable_proto->mutable_bitmap();
        bitmap_proto->set_url(proto.image_path());
        bitmap_proto->set_use_instrinsic_dimensions(true);
        use_instrinsic_dimensions = true;
      }
      break;
    }
    case InfoBoxProto::ImageCase::kDrawable: {
      drawable_proto = proto.drawable();
      break;
    }
    case InfoBoxProto::ImageCase::IMAGE_NOT_SET: {
      break;
    }
  }
  base::android::ScopedJavaLocalRef<jobject> jdrawable = nullptr;
  if (drawable_proto.has_value()) {
    jdrawable = ui_controller_android_utils::CreateJavaDrawable(
        env, jcontext, *dependencies_, *drawable_proto,
        execution_delegate_->GetUserModel());
  }
  auto jinfo_box = Java_AssistantInfoBox_create(
      env, jdrawable, ConvertUTF8ToJavaString(env, proto.explanation()),
      use_instrinsic_dimensions);
  Java_AssistantInfoBoxModel_setInfoBox(env, jmodel, jinfo_box);
}

void UiControllerAndroid::Stop(JNIEnv* env,
                               const base::android::JavaParamRef<jobject>& obj,
                               int jreason) {
  client_->Shutdown(static_cast<Metrics::DropOutReason>(jreason));
}

void UiControllerAndroid::OnFatalError(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jstring>& jmessage,
    int jreason) {
  if (!execution_delegate_)
    return;

  execution_delegate_->OnFatalError(
      base::android::ConvertJavaStringToUTF8(env, jmessage),
      static_cast<Metrics::DropOutReason>(jreason));
}

void UiControllerAndroid::ResetGenericUiControllers() {
  JNIEnv* env = AttachCurrentThread();
  collect_user_data_prepended_generic_ui_controller_.reset();
  collect_user_data_appended_generic_ui_controller_.reset();
  generic_ui_controller_.reset();
  auto jcollectuserdatamodel = GetCollectUserDataModel();
  Java_AssistantCollectUserDataModel_setGenericUserInterfacePrepended(
      env, jcollectuserdatamodel, nullptr);
  Java_AssistantCollectUserDataModel_setGenericUserInterfaceAppended(
      env, jcollectuserdatamodel, nullptr);
  Java_AssistantGenericUiModel_setView(env, GetGenericUiModel(), nullptr);
}

std::unique_ptr<GenericUiRootControllerAndroid>
UiControllerAndroid::CreateGenericUiControllerForProto(
    const GenericUserInterfaceProto& proto) {
  JNIEnv* env = AttachCurrentThread();
  auto jcontext =
      Java_AutofillAssistantUiController_getContext(env, java_object_);
  return GenericUiRootControllerAndroid::CreateFromProto(
      proto, base::android::ScopedJavaGlobalRef<jobject>(jcontext),
      GetInfoPageUtil(), *dependencies_, generic_ui_delegate_.GetJavaObject(),
      ui_delegate_->GetEventHandler(), execution_delegate_->GetUserModel(),
      ui_delegate_->GetBasicInteractions());
}

base::android::ScopedJavaLocalRef<jobject>
UiControllerAndroid::GetGenericUiModel() {
  return Java_AssistantModel_getGenericUiModel(AttachCurrentThread(),
                                               GetModel());
}

base::android::ScopedJavaLocalRef<jobject>
UiControllerAndroid::GetPersistentGenericUiModel() {
  return Java_AssistantModel_getPersistentGenericUiModel(AttachCurrentThread(),
                                                         GetModel());
}

// Creates the java equivalent to |sections|.
base::android::ScopedJavaLocalRef<jobject>
UiControllerAndroid::CreateJavaAdditionalSections(
    JNIEnv* env,
    const std::vector<UserFormSectionProto>& sections) {
  auto jsection_list =
      Java_AssistantCollectUserDataModel_createAdditionalSectionsList(env);
  for (const auto& section : sections) {
    switch (section.section_case()) {
      case UserFormSectionProto::kStaticTextSection: {
        std::string text;
        switch (section.static_text_section().value_case()) {
          case StaticTextSectionProto::kClientMemoryKey: {
            user_data::GetClientMemoryStringValue(
                section.static_text_section().client_memory_key(),
                execution_delegate_->GetUserData(),
                execution_delegate_->GetUserModel(), &text);
            break;
          }

          case StaticTextSectionProto::kText:
          case StaticTextSectionProto::VALUE_NOT_SET: {
            text = section.static_text_section().text();
          }
        }
        Java_AssistantCollectUserDataModel_appendStaticTextSection(
            env, jsection_list, ConvertUTF8ToJavaString(env, section.title()),
            ConvertUTF8ToJavaString(env, text));
        break;
      }
      case UserFormSectionProto::kTextInputSection: {
        Java_AssistantCollectUserDataModel_appendTextInputSection(
            env, jsection_list, ConvertUTF8ToJavaString(env, section.title()),
            CreateJavaTextInputsForSection(env, section.text_input_section()));
        break;
      }
      case UserFormSectionProto::kPopupListSection: {
        std::vector<std::string> items;
        base::ranges::copy(section.popup_list_section().item_names(),
                           std::back_inserter(items));
        std::vector<int> initial_selections;
        base::ranges::copy(section.popup_list_section().initial_selection(),
                           std::back_inserter(initial_selections));
        Java_AssistantCollectUserDataModel_appendPopupListSection(
            env, jsection_list, ConvertUTF8ToJavaString(env, section.title()),
            ConvertUTF8ToJavaString(
                env, section.popup_list_section().additional_value_key()),
            base::android::ToJavaArrayOfStrings(env, items),
            base::android::ToJavaIntArray(env, initial_selections),
            section.popup_list_section().allow_multiselect(),
            section.popup_list_section().selection_mandatory(),
            ConvertUTF8ToJavaString(
                env,
                section.popup_list_section().no_selection_error_message()));
        break;
      }
      case UserFormSectionProto::SECTION_NOT_SET:
        NOTREACHED();
        break;
    }
  }
  return jsection_list;
}

}  // namespace autofill_assistant
