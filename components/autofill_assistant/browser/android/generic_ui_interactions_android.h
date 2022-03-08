// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ANDROID_GENERIC_UI_INTERACTIONS_ANDROID_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ANDROID_GENERIC_UI_INTERACTIONS_ANDROID_H_

#include <string>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/android/interaction_handler_android.h"
#include "components/autofill_assistant/browser/basic_interactions.h"
#include "components/autofill_assistant/browser/generic_ui.pb.h"

namespace autofill_assistant {
class RadioButtonController;
class ViewHandlerAndroid;

namespace android_interactions {

// Writes a value to the model.
void SetValue(base::WeakPtr<BasicInteractions> basic_interactions,
              const SetModelValueProto& proto);

// Computes a value and writes it to the model.
void ComputeValue(base::WeakPtr<BasicInteractions> basic_interactions,
                  const ComputeValueProto& proto);

// Sets the list of available user actions (i.e., chips and direct actions).
void SetUserActions(base::WeakPtr<BasicInteractions> basic_interactions,
                    const SetUserActionsProto& proto);

// Ends the current ShowGenericUi action.
void EndAction(base::WeakPtr<BasicInteractions> basic_interactions,
               const EndActionProto& proto);

// Enables or disables a particular user action.
void ToggleUserAction(base::WeakPtr<BasicInteractions> basic_interactions,
                      const ToggleUserActionProto& proto);

// Displays an info popup on the screen.
// close_display_str is used to show a button, if not specified in proto.
void ShowInfoPopup(const InfoPopupProto& proto,
                   base::android::ScopedJavaGlobalRef<jobject> jcontext,
                   base::android::ScopedJavaGlobalRef<jobject> jinfo_page_util,
                   const std::string& close_display_str);

// Displays a list popup on the screen.
void ShowListPopup(base::WeakPtr<UserModel> user_model,
                   const ShowListPopupProto& proto,
                   base::android::ScopedJavaGlobalRef<jobject> jcontext,
                   base::android::ScopedJavaGlobalRef<jobject> jdelegate);

// Displays a calendar popup on the screen.
void ShowCalendarPopup(base::WeakPtr<UserModel> user_model,
                       const ShowCalendarPopupProto& proto,
                       base::android::ScopedJavaGlobalRef<jobject> jcontext,
                       base::android::ScopedJavaGlobalRef<jobject> jdelegate);

// Displays a generic popup on the screen.
void ShowGenericPopup(const ShowGenericUiPopupProto& proto,
                      base::android::ScopedJavaGlobalRef<jobject> jcontent_view,
                      base::android::ScopedJavaGlobalRef<jobject> jcontext,
                      base::android::ScopedJavaGlobalRef<jobject> jdelegate);

// Sets the text of a view.
void SetViewText(base::WeakPtr<UserModel> user_model,
                 const SetTextProto& proto,
                 ViewHandlerAndroid* view_handler,
                 base::android::ScopedJavaGlobalRef<jobject> jdelegate);

// Sets the visibility of a view.
void SetViewVisibility(base::WeakPtr<UserModel> user_model,
                       const SetViewVisibilityProto& proto,
                       ViewHandlerAndroid* view_handler);

// Enables or disables a view.
void SetViewEnabled(base::WeakPtr<UserModel> user_model,
                    const SetViewEnabledProto& proto,
                    ViewHandlerAndroid* view_handler);

// A simple wrapper around a basic interaction, needed because we can't directly
// bind a repeating callback to a method with non-void return value.
void RunConditionalCallback(
    base::WeakPtr<BasicInteractions> basic_interactions,
    const std::string& condition_identifier,
    InteractionHandlerAndroid::InteractionCallback callback);

// Sets the checked state of a toggle button.
void SetToggleButtonChecked(base::WeakPtr<UserModel> user_model,
                            const std::string& view_identifier,
                            const std::string& model_identifier,
                            ViewHandlerAndroid* view_handler);

// Removes all child views from |view_identifier|.
void ClearViewContainer(const std::string& view_identifier,
                        ViewHandlerAndroid* view_handler,
                        base::android::ScopedJavaGlobalRef<jobject> jdelegate);

// Attaches |jview| to a parent view.
bool AttachViewToParent(base::android::ScopedJavaGlobalRef<jobject> jview,
                        const std::string& parent_view_identifier,
                        ViewHandlerAndroid* view_handler);

void UpdateRadioButtonGroup(
    base::WeakPtr<RadioButtonController> radio_button_controller,
    const std::string& radio_group,
    const std::string& model_identifier);

}  // namespace android_interactions
}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ANDROID_GENERIC_UI_INTERACTIONS_ANDROID_H_
