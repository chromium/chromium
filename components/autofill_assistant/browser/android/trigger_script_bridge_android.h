// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ANDROID_TRIGGER_SCRIPT_BRIDGE_ANDROID_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ANDROID_TRIGGER_SCRIPT_BRIDGE_ANDROID_H_

#include "base/android/jni_android.h"
#include "base/memory/raw_ptr.h"
#include "components/autofill_assistant/browser/android/dependencies.h"
#include "components/autofill_assistant/browser/metrics.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/trigger_context.h"
#include "components/autofill_assistant/browser/trigger_scripts/trigger_script_coordinator.h"
#include "url/gurl.h"

namespace autofill_assistant {

// Facilitates communication between trigger script UI and native coordinator.
class TriggerScriptBridgeAndroid : public TriggerScriptCoordinator::UiDelegate {
 public:
  TriggerScriptBridgeAndroid(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& jweb_contents,
      const base::android::JavaRef<jobject>& jassistant_deps);
  ~TriggerScriptBridgeAndroid() override;
  TriggerScriptBridgeAndroid(const TriggerScriptBridgeAndroid&) = delete;
  TriggerScriptBridgeAndroid& operator=(const TriggerScriptBridgeAndroid&) =
      delete;

  // Implements TriggerScriptCoordinator::UiDelegate:
  void ShowTriggerScript(const TriggerScriptUIProto& proto) override;
  void HideTriggerScript() override;
  void Attach(TriggerScriptCoordinator* trigger_script_coordinator) override;
  void Detach() override;

  // Called by the UI to execute a specific trigger script action.
  void OnTriggerScriptAction(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      jint action);

  // Called by the UI when the bottom sheet has been swipe-dismissed.
  void OnBottomSheetClosedWithSwipe(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller);

  // Called by the UI when the back button was pressed. Returns whether the
  // event was handled or not.
  bool OnBackButtonPressed(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& jcaller);

  // Called by the UI when the keyboard was shown or hidden.
  void OnKeyboardVisibilityChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      jboolean jvisible);

 private:
  // Reference to the Java counterpart to this class.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
  // Pointer to the native coordinator. Only set while attached.
  raw_ptr<TriggerScriptCoordinator> trigger_script_coordinator_ = nullptr;

  // Java-side AssistantStaticDependencies object. This never changes during the
  // life of the application.
  const std::unique_ptr<const Dependencies> dependencies_;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ANDROID_TRIGGER_SCRIPT_BRIDGE_ANDROID_H_
