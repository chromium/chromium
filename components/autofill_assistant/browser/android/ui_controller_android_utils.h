// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ANDROID_UI_CONTROLLER_ANDROID_UTILS_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ANDROID_UI_CONTROLLER_ANDROID_UTILS_H_

#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/containers/flat_map.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill_assistant/browser/android/dependencies_android.h"
#include "components/autofill_assistant/browser/autofill_assistant_tts_controller.h"
#include "components/autofill_assistant/browser/bottom_sheet_state.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/trigger_context.h"
#include "components/autofill_assistant/browser/user_model.h"
#include "components/autofill_assistant/browser/view_layout.pb.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace autofill_assistant {
class Service;
class ServiceRequestSender;
class ClientAndroid;

namespace ui_controller_android_utils {

// Returns a 32-bit Integer representing |color_string| in Java, or null if
// |color_string| is invalid.
// TODO(806868): Get rid of this overload and always use GetJavaColor(proto).
base::android::ScopedJavaLocalRef<jobject> GetJavaColor(
    JNIEnv* env,
    const std::string& color_string);

// Returns a 32-bit Integer representing |proto| in Java, or null if
// |proto| is invalid.
base::android::ScopedJavaLocalRef<jobject> GetJavaColor(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jcontext,
    const ColorProto& proto);

// Returns the pixelsize of |proto| in |jcontext|, or |nullopt| if |proto| is
// invalid.
absl::optional<int> GetPixelSize(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jcontext,
    const ClientDimensionProto& proto);

// Returns the pixelsize of |proto| in |jcontext|, or |default_value| if |proto|
// is invalid.
int GetPixelSizeOrDefault(JNIEnv* env,
                          const base::android::JavaRef<jobject>& jcontext,
                          const ClientDimensionProto& proto,
                          int default_value);

// Returns an instance of an |AssistantDrawable| or nullptr if it could not
// be created.
base::android::ScopedJavaLocalRef<jobject> CreateJavaDrawable(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jcontext,
    const DependenciesAndroid& dependencies,
    const DrawableProto& proto,
    const UserModel* user_model = nullptr);

// Returns the java equivalent of |proto|.
base::android::ScopedJavaLocalRef<jobject> ToJavaValue(JNIEnv* env,
                                                       const ValueProto& proto);

// Returns the native equivalent of |jvalue|. Returns an empty ValueProto if
// |jvalue| is null.
ValueProto ToNativeValue(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& jvalue);

// Returns an instance of |AssistantInfoPopup| for |proto|.
// close_display_str is used to provide a Close button when a button is not
// configured in proto.
base::android::ScopedJavaLocalRef<jobject> CreateJavaInfoPopup(
    JNIEnv* env,
    const InfoPopupProto& proto,
    const base::android::JavaRef<jobject>& jinfo_page_util,
    const std::string& close_display_str);

// Shows an instance of |AssistantInfoPopup| on the screen.
void ShowJavaInfoPopup(JNIEnv* env,
                       const base::android::JavaRef<jobject>& jinfo_popup,
                       const base::android::JavaRef<jobject>& jcontext);

// Converts a java string to native. Returns an empty string if input is null.
std::string SafeConvertJavaStringToNative(
    JNIEnv* env,
    const base::android::JavaRef<jstring>& jstring);

// Converts an optional native string to java. Returns null if the optional is
// not present.
base::android::ScopedJavaLocalRef<jstring> ConvertNativeOptionalStringToJava(
    JNIEnv* env,
    const absl::optional<std::string> optional_string);

// Creates a BottomSheetState from the Android SheetState enum defined in
// components/browser_ui/bottomsheet/BottomSheetController.java.
BottomSheetState ToNativeBottomSheetState(int state);

// Converts a BottomSheetState to the Android SheetState enum.
int ToJavaBottomSheetState(BottomSheetState state);

// Returns an instance of |AssistantChip| or nullptr if the chip type is
// invalid.
base::android::ScopedJavaLocalRef<jobject> CreateJavaAssistantChip(
    JNIEnv* env,
    const ChipProto& chip);

// Returns a list of |AssistantChip| instances or nullptr if any of the chips
// in |chips| has an invalid type.
base::android::ScopedJavaLocalRef<jobject> CreateJavaAssistantChipList(
    JNIEnv* env,
    const std::vector<ChipProto>& chips);

// Creates a base::flat_map from an incoming set of Java string keys and values.
base::flat_map<std::string, std::string> CreateStringMapFromJava(
    JNIEnv* env,
    const base::android::JavaRef<jobjectArray>& keys,
    const base::android::JavaRef<jobjectArray>& values);

// Creates a C++ trigger context for the specified java inputs.
std::unique_ptr<TriggerContext> CreateTriggerContext(
    JNIEnv* env,
    content::WebContents* web_contents,
    const base::android::JavaRef<jstring>& jexperiment_ids,
    const base::android::JavaRef<jobjectArray>& jparameter_names,
    const base::android::JavaRef<jobjectArray>& jparameter_values,
    const base::android::JavaRef<jobjectArray>& jdevice_only_parameter_names,
    const base::android::JavaRef<jobjectArray>& jdevice_only_parameter_values,
    jboolean onboarding_shown,
    jboolean is_direct_action,
    const base::android::JavaRef<jstring>& jinitial_url,
    const bool is_custom_tab);

// Returns the service to inject, if any, for |client_android|. This is used for
// integration tests, which provide a test service to communicate with.
std::unique_ptr<Service> GetServiceToInject(JNIEnv* env,
                                            ClientAndroid* client_android);

// Returns the service request sender to inject, if any. This is used for
// integration tests which provide a test service request sender to communicate
// with.
std::unique_ptr<ServiceRequestSender> GetServiceRequestSenderToInject(
    JNIEnv* env);

// Returns the TTS controller to inject, if any. This is used for integration
// tests which provide a test TTS controller.
std::unique_ptr<AutofillAssistantTtsController> GetTtsControllerToInject(
    JNIEnv* env);

// Creates an AssistantAutofillProfile in Java. This is comparable to
// PersonalDataManagerAndroid::CreateJavaProfileFromNative.
base::android::ScopedJavaLocalRef<jobject> CreateAssistantAutofillProfile(
    JNIEnv* env,
    const autofill::AutofillProfile& profile,
    const std::string& locale);

// Populate the AutofillProfile from the Java AssistantAutofillProfile. This is
// comparable to PersonalDataManagerAndroid::PopulateNativeProfileFromJava.
void PopulateAutofillProfileFromJava(
    const base::android::JavaParamRef<jobject>& jprofile,
    JNIEnv* env,
    autofill::AutofillProfile* profile,
    const std::string& locale);

// Creates an AssistantAutofillCreditCard in Java. This is comparable to
// PersonalDataManagerAndroid::CreateJavaCreditCardFromNative.
base::android::ScopedJavaLocalRef<jobject> CreateAssistantAutofillCreditCard(
    JNIEnv* env,
    const autofill::CreditCard& credit_card,
    const std::string& locale);

// Populate the CreditCard from the Java AssistantAutofillCreditCard. This is
// comparable to PersonalDataManagerAndroid::PopulateNativeCreditCardFromJava.
void PopulateAutofillCreditCardFromJava(
    const base::android::JavaParamRef<jobject>& jcredit_card,
    JNIEnv* env,
    autofill::CreditCard* credit_card,
    const std::string& locale);

}  // namespace ui_controller_android_utils
}  //  namespace autofill_assistant

#endif  //  COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ANDROID_UI_CONTROLLER_ANDROID_UTILS_H_
