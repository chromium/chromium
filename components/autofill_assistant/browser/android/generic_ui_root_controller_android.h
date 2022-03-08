// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ANDROID_GENERIC_UI_ROOT_CONTROLLER_ANDROID_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ANDROID_GENERIC_UI_ROOT_CONTROLLER_ANDROID_H_

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "components/autofill_assistant/browser/android/dependencies.h"
#include "components/autofill_assistant/browser/service.pb.h"

namespace autofill_assistant {
class BasicInteractions;
class EventHandler;
class RadioButtonController;
class UserModel;
class GenericUiNestedControllerAndroid;

class GenericUiRootControllerAndroid {
 public:
  // Attempts to create a new instance. May fail if the proto is invalid.
  // Arguments must outlive this instance. Ownership of the arguments is not
  // changed.
  static std::unique_ptr<GenericUiRootControllerAndroid> CreateFromProto(
      const GenericUserInterfaceProto& proto,
      base::android::ScopedJavaGlobalRef<jobject> jcontext,
      base::android::ScopedJavaGlobalRef<jobject> jinfo_page_util,
      const Dependencies& dependencies,
      base::android::ScopedJavaGlobalRef<jobject> jdelegate,
      EventHandler* event_handler,
      UserModel* user_model,
      BasicInteractions* basic_interactions);

  base::android::ScopedJavaGlobalRef<jobject> GetRootView() const;

  GenericUiRootControllerAndroid(
      std::unique_ptr<RadioButtonController> radio_button_controller,
      std::unique_ptr<GenericUiNestedControllerAndroid> controller);
  ~GenericUiRootControllerAndroid();
  GenericUiRootControllerAndroid(const GenericUiRootControllerAndroid&) =
      delete;
  GenericUiRootControllerAndroid& operator=(GenericUiRootControllerAndroid&) =
      delete;

 private:
  // Note that |controller_| must be destroyed before
  // |radio_button_controller_|, since the destructor of |controller_| or any of
  // the nested instances inside |controller_| may need to unregister radio
  // buttons. The order of members is not arbitrary here.
  std::unique_ptr<RadioButtonController> radio_button_controller_;
  std::unique_ptr<GenericUiNestedControllerAndroid> controller_;
};

}  //  namespace autofill_assistant

#endif  //  COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ANDROID_GENERIC_UI_ROOT_CONTROLLER_ANDROID_H_
