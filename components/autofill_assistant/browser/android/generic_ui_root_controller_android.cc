// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/android/generic_ui_root_controller_android.h"

#include "components/autofill_assistant/browser/android/dependencies_android.h"
#include "components/autofill_assistant/browser/android/generic_ui_nested_controller_android.h"
#include "components/autofill_assistant/browser/radio_button_controller.h"

namespace autofill_assistant {

GenericUiRootControllerAndroid::GenericUiRootControllerAndroid(
    std::unique_ptr<RadioButtonController> radio_button_controller,
    std::unique_ptr<GenericUiNestedControllerAndroid> controller)
    : radio_button_controller_(std::move(radio_button_controller)),
      controller_(std::move(controller)) {}

GenericUiRootControllerAndroid::~GenericUiRootControllerAndroid() = default;

base::android::ScopedJavaGlobalRef<jobject>
GenericUiRootControllerAndroid::GetRootView() const {
  return controller_->GetRootView();
}

// static
std::unique_ptr<GenericUiRootControllerAndroid>
GenericUiRootControllerAndroid::CreateFromProto(
    const GenericUserInterfaceProto& proto,
    base::android::ScopedJavaGlobalRef<jobject> jcontext,
    base::android::ScopedJavaGlobalRef<jobject> jinfo_page_util,
    const DependenciesAndroid& dependencies,
    base::android::ScopedJavaGlobalRef<jobject> jdelegate,
    EventHandler* event_handler,
    UserModel* user_model,
    BasicInteractions* basic_interactions) {
  auto radio_button_controller =
      std::make_unique<RadioButtonController>(user_model);
  auto controller = GenericUiNestedControllerAndroid::CreateFromProto(
      proto, jcontext, jinfo_page_util, dependencies, jdelegate, event_handler,
      user_model, basic_interactions, radio_button_controller.get());

  if (controller == nullptr) {
    return nullptr;
  }

  return std::make_unique<GenericUiRootControllerAndroid>(
      std::move(radio_button_controller), std::move(controller));
}

}  // namespace autofill_assistant
