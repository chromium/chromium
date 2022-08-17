// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ANDROID_UI_CONTROLLER_ANDROID_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ANDROID_UI_CONTROLLER_ANDROID_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "components/autofill_assistant/browser/android/assistant_bottom_bar_delegate.h"
#include "components/autofill_assistant/browser/android/assistant_collect_user_data_delegate.h"
#include "components/autofill_assistant/browser/android/assistant_form_delegate.h"
#include "components/autofill_assistant/browser/android/assistant_generic_ui_delegate.h"
#include "components/autofill_assistant/browser/android/assistant_header_delegate.h"
#include "components/autofill_assistant/browser/android/assistant_header_model.h"
#include "components/autofill_assistant/browser/android/assistant_overlay_delegate.h"
#include "components/autofill_assistant/browser/android/assistant_qr_code_camera_scan_model_wrapper.h"
#include "components/autofill_assistant/browser/android/assistant_qr_code_image_picker_model_wrapper.h"
#include "components/autofill_assistant/browser/android/assistant_qr_code_native_delegate.h"
#include "components/autofill_assistant/browser/android/dependencies_android.h"
#include "components/autofill_assistant/browser/chip.h"
#include "components/autofill_assistant/browser/details.h"
#include "components/autofill_assistant/browser/empty_controller_observer.h"
#include "components/autofill_assistant/browser/execution_delegate.h"
#include "components/autofill_assistant/browser/info_box.h"
#include "components/autofill_assistant/browser/metrics.h"
#include "components/autofill_assistant/browser/overlay_state.h"
#include "components/autofill_assistant/browser/trigger_context.h"
#include "components/autofill_assistant/browser/ui_controller_observer.h"
#include "components/autofill_assistant/browser/user_action.h"

namespace autofill_assistant {
struct ClientSettings;
class GenericUiRootControllerAndroid;
class ClientAndroid;

// Starts and owns the UI elements required to display AA.
//
// This class and its UI elements are tied to a ChromeActivity. A
// UiControllerAndroid can be attached and detached from an AA controller, which
// is tied to a BrowserContent.
//
// TODO(crbug.com/806868): This class should be renamed to
// AssistantMediator(Android) and listen for state changes to forward those
// changes to the UI model.
class UiControllerAndroid : public EmptyControllerObserver,
                            UiControllerObserver {
 public:
  static std::unique_ptr<UiControllerAndroid> CreateFromWebContents(
      content::WebContents* web_contents,
      const base::android::JavaRef<jobject>& jdependencies,
      const base::android::JavaRef<jobject>& joverlay_coordinator);

  UiControllerAndroid(
      JNIEnv* env,
      content::WebContents* web_contents,
      const base::android::JavaRef<jobject>& jdependencies,
      const base::android::JavaRef<jobject>& joverlay_coordinator);

  UiControllerAndroid(const UiControllerAndroid&) = delete;
  UiControllerAndroid& operator=(const UiControllerAndroid&) = delete;

  ~UiControllerAndroid() override;

  // Attaches the UI to the given client, its web contents and delegate.
  //
  // |web_contents|, |client|, |execution_delegate| and |ui_delegate| must
  // remain valid for the lifetime of this instance or until Attach() is called
  // again, with different pointers.
  void Attach(content::WebContents* web_contents,
              ClientAndroid* client,
              ExecutionDelegate* execution_delegate,
              UiDelegate* ui_delegate);

  // Detaches the UI from |execution_delegate| and |ui_delegate_|. It will stop
  // receiving notifications from the delegates until it is attached again.
  void Detach();

  // Returns true if the UI is attached to an execution delegate.
  bool IsAttached() { return execution_delegate_ != nullptr; }

  // Returns whether the UI is currently attached to the given execution
  // delegate or not.
  bool IsAttachedTo(ExecutionDelegate* execution_delegate) {
    return execution_delegate_ == execution_delegate;
  }

  // Have the UI react as if a close or cancel button was pressed.
  //
  // If action_index != -1, execute that action as close/cancel. Otherwise
  // execute the default close or cancel action.
  void CloseOrCancel(int action_index, Metrics::DropOutReason dropout_reason);
  // Returns the size of the window.
  absl::optional<std::pair<int, int>> GetWindowSize() const;
  // Returns the screen's orientation.
  ClientContextProto::ScreenOrientation GetScreenOrientation() const;

  // Overrides ControllerObserver:
  void OnStateChanged(AutofillAssistantState new_state) override;
  void OnKeyboardSuppressionStateChanged(
      bool should_suppress_keyboard) override;
  void CloseCustomTab() override;
  void OnUserDataChanged(const UserData& user_data,
                         UserDataFieldChange field_change) override;
  void OnTouchableAreaChanged(
      const RectF& visual_viewport,
      const std::vector<RectF>& touchable_areas,
      const std::vector<RectF>& restricted_areas) override;
  void OnViewportModeChanged(ViewportMode mode) override;
  void OnOverlayColorsChanged(
      const ExecutionDelegate::OverlayColors& colors) override;
  void OnClientSettingsChanged(const ClientSettings& settings) override;
  void OnShouldShowOverlayChanged(bool should_show) override;

  // Overrides UiControllerObserver:
  void OnStatusMessageChanged(const std::string& message) override;
  void OnBubbleMessageChanged(const std::string& message) override;
  void OnUserActionsChanged(const std::vector<UserAction>& actions) override;
  void OnCollectUserDataOptionsChanged(
      const CollectUserDataOptions* collect_user_data_options) override;
  void OnCollectUserDataUiStateChanged(bool loading,
                                       UserDataEventField event_field) override;
  void OnDetailsChanged(const std::vector<Details>& details) override;
  void OnInfoBoxChanged(const InfoBox* info_box) override;
  void OnProgressActiveStepChanged(int active_step) override;
  void OnProgressVisibilityChanged(bool visible) override;
  void OnProgressBarErrorStateChanged(bool error) override;
  void OnStepProgressBarConfigurationChanged(
      const ShowProgressBarProto::StepProgressBarConfiguration& configuration)
      override;
  void OnPeekModeChanged(
      ConfigureBottomSheetProto::PeekMode peek_mode) override;
  void OnExpandBottomSheet() override;
  void OnCollapseBottomSheet() override;
  void OnFormChanged(const FormProto* form,
                     const FormProto::Result* result) override;
  void OnQrCodeScanUiChanged(
      const PromptQrCodeScanProto* qr_code_scan) override;
  void OnGenericUserInterfaceChanged(
      const GenericUserInterfaceProto* generic_ui) override;
  void OnPersistentGenericUserInterfaceChanged(
      const GenericUserInterfaceProto* generic_ui) override;
  void OnTtsButtonVisibilityChanged(bool visible) override;
  void OnTtsButtonStateChanged(TtsButtonState state) override;

  // Called by AssistantOverlayDelegate:
  void OnUnexpectedTaps();
  void OnUserInteractionInsideTouchableArea();

  // Called by AssistantHeaderDelegate:
  void OnHeaderFeedbackButtonClicked();
  void OnTtsButtonClicked();

  // Called by AssistantQrCodeDelegate.
  void OnQrCodeScanFinished(const ClientStatus& status,
                            const absl::optional<ValueProto>& value);

  // Called by AssistantGenericUiDelegate:
  void OnViewEvent(const EventHandler::EventKey& key);
  void OnValueChanged(const std::string& identifier, const ValueProto& value);

  // Called by AssistantCollectUserDataDelegate:
  void OnShippingAddressChanged(
      std::unique_ptr<autofill::AutofillProfile> address,
      UserDataEventType event_type);
  void OnContactInfoChanged(std::unique_ptr<autofill::AutofillProfile> profile,
                            UserDataEventType event_type);
  void OnPhoneNumberChanged(std::unique_ptr<autofill::AutofillProfile> profile,
                            UserDataEventType event_type);
  void OnCreditCardChanged(
      std::unique_ptr<autofill::CreditCard> card,
      std::unique_ptr<autofill::AutofillProfile> billing_profile,
      UserDataEventType event_type);
  void OnTermsAndConditionsChanged(TermsAndConditionsState state);
  void OnLoginChoiceChanged(const std::string& identifier);
  void OnTextLinkClicked(int link);
  void OnFormActionLinkClicked(int link);
  void OnKeyValueChanged(const std::string& key, const ValueProto& value);
  void OnInputTextFocusChanged(bool is_text_focused);

  // Called by AssistantFormDelegate:
  void OnCounterChanged(int input_index, int counter_index, int value);
  void OnChoiceSelectionChanged(int input_index,
                                int choice_index,
                                bool selected);

  // Called by AssistantBottomBarNativeDelegate:
  bool OnBackButtonClicked();
  void OnBottomSheetClosedWithSwipe();

  // Called by Java.
  void SnackbarResult(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj,
                      jboolean undo);
  void Stop(JNIEnv* env,
            const base::android::JavaParamRef<jobject>& obj,
            int reason);
  void OnFatalError(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj,
                    const base::android::JavaParamRef<jstring>& message,
                    int reason);
  base::android::ScopedJavaLocalRef<jstring> GetPrimaryAccountName(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller);
  void OnUserActionSelected(JNIEnv* env,
                            const base::android::JavaParamRef<jobject>& jcaller,
                            jint index);
  void OnCancelButtonClicked(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      jint actionIndex);
  void OnCloseButtonClicked(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller);
  void OnFeedbackButtonClicked(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      jint actionIndex);
  void OnKeyboardVisibilityChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      jboolean visible);
  void SetVisible(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& jcaller,
                  jboolean visible);
  void OnTabSwitched(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& jcaller,
                     jint state,
                     jboolean activity_changed);
  void OnTabSelected(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& jcaller);

 private:
  // A pointer to the client. nullptr until Attach() is called.
  raw_ptr<ClientAndroid> client_ = nullptr;

  // Pointers to the execution_delegate and ui_delegate. nullptr until Attach()
  // is called.
  raw_ptr<ExecutionDelegate> execution_delegate_ = nullptr;
  raw_ptr<UiDelegate> ui_delegate_ = nullptr;
  AssistantOverlayDelegate overlay_delegate_;
  AssistantHeaderDelegate header_delegate_;
  AssistantCollectUserDataDelegate collect_user_data_delegate_;
  AssistantFormDelegate form_delegate_;
  AssistantGenericUiDelegate generic_ui_delegate_;
  AssistantBottomBarDelegate bottom_bar_delegate_;

  // What to do if undo is not pressed on the current snackbar.
  base::OnceCallback<void()> snackbar_action_;

  base::android::ScopedJavaLocalRef<jobject> GetModel();
  base::android::ScopedJavaLocalRef<jobject> GetOverlayModel();
  base::android::ScopedJavaLocalRef<jobject> GetHeaderModel();
  base::android::ScopedJavaLocalRef<jobject> GetDetailsModel();
  base::android::ScopedJavaLocalRef<jobject> GetInfoBoxModel();
  base::android::ScopedJavaLocalRef<jobject> GetCollectUserDataModel();
  base::android::ScopedJavaLocalRef<jobject> GetFormModel();
  base::android::ScopedJavaLocalRef<jobject> GetGenericUiModel();
  base::android::ScopedJavaLocalRef<jobject> GetPersistentGenericUiModel();
  base::android::ScopedJavaLocalRef<jobject> CreateJavaAdditionalSections(
      JNIEnv* env,
      const std::vector<UserFormSectionProto>& sections);

  // The ExecutionDelegate has the last say on whether we should show the
  // overlay. This saves the AutofillAssistantState-determined OverlayState and
  // then applies it the actual UI only if the ExecutionDelegate's
  // ShouldShowOverlay is true.
  void SetOverlayState(OverlayState state);
  // Applies the specified OverlayState to the UI.
  void ApplyOverlayState(OverlayState state);
  void ShowContentAndExpandBottomSheet();
  void SetSpinPoodle(bool enabled);
  std::string GetDebugContext();
  void DestroySelf();
  void Shutdown(Metrics::DropOutReason reason);
  void UpdateActions(const std::vector<UserAction>& GetUserActions);
  void HideKeyboardIfFocusNotOnText();

  base::android::ScopedJavaGlobalRef<jobject> GetInfoPageUtil() const;

  void ResetGenericUiControllers();
  std::unique_ptr<GenericUiRootControllerAndroid>
  CreateGenericUiControllerForProto(const GenericUserInterfaceProto& proto);

  // Hide the UI, show a snackbar with an undo button, and execute the given
  // action after a short delay unless the user taps the undo button.
  void ShowSnackbar(base::TimeDelta delay,
                    const std::string& message,
                    const std::string& undo_string,
                    base::OnceCallback<void()> action);

  void OnCancel(int action_index, Metrics::DropOutReason dropout_reason);

  // Updates the state of the UI to reflect the ExecutionDelegate's state.
  void SetupForState();

  // Makes the whole of AA invisible or visible again.
  void SetVisible(bool visible);

  // Restore the UI for the current ExecutionDelegate.
  void RestoreUi();

  // Performs tasks to update Display String Changes.
  void OnClientSettingsDisplayStringsChanged(const ClientSettings& settings);

  // Java-side AutofillAssistantUiController object.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  // Java-side AssistantStaticDependencies object. This never changes during the
  // life of the application.
  const std::unique_ptr<const DependenciesAndroid> dependencies_;

  // Native controllers for generic UI.
  std::unique_ptr<GenericUiRootControllerAndroid>
      collect_user_data_prepended_generic_ui_controller_;
  std::unique_ptr<GenericUiRootControllerAndroid>
      collect_user_data_appended_generic_ui_controller_;
  std::unique_ptr<GenericUiRootControllerAndroid> generic_ui_controller_;
  std::unique_ptr<GenericUiRootControllerAndroid>
      persistent_generic_ui_controller_;

  OverlayState desired_overlay_state_ = OverlayState::FULL;
  OverlayState overlay_state_ = OverlayState::FULL;

  std::unique_ptr<AssistantHeaderModel> header_model_;

  std::unique_ptr<AssistantQrCodeNativeDelegate> qr_code_native_delegate_;
  std::unique_ptr<AssistantQrCodeCameraScanModelWrapper>
      qr_code_camera_scan_model_wrapper_;
  std::unique_ptr<AssistantQrCodeImagePickerModelWrapper>
      qr_code_image_picker_model_wrapper_;

  base::WeakPtrFactory<UiControllerAndroid> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant.
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ANDROID_UI_CONTROLLER_ANDROID_H_
