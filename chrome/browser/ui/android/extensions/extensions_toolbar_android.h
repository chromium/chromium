// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_EXTENSIONS_EXTENSIONS_TOOLBAR_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_EXTENSIONS_EXTENSIONS_TOOLBAR_ANDROID_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/extensions/extensions_toolbar_view_model.h"
#include "third_party/jni_zero/jni_zero.h"

class BrowserWindowInterface;

namespace extensions {

// The JNI bridge for the extensions UI.
// This bridge is created and owned by Java UI code.
class ExtensionsToolbarAndroid : public ExtensionsToolbarViewModel::Delegate,
                                 public ExtensionsToolbarViewModel::Observer {
 public:
  ExtensionsToolbarAndroid(BrowserWindowInterface* browser,
                           const base::android::JavaRef<jobject>& java_object);
  ExtensionsToolbarAndroid(const ExtensionsToolbarAndroid&) = delete;
  ExtensionsToolbarAndroid& operator=(const ExtensionsToolbarAndroid&) = delete;
  ~ExtensionsToolbarAndroid() override;

  // Triggers the display of an extension popup in the Java UI.
  void TriggerPopup(const ToolbarActionsModel::ActionId& action_id,
                    std::unique_ptr<ExtensionViewHost> host);

  // ExtensionsToolbarViewModel::Delegate:
  std::unique_ptr<ExtensionActionViewModel> CreateActionViewModel(
      const ToolbarActionsModel::ActionId& action_id,
      ExtensionsContainer* extensions_container) override;
  void HideActivePopup() override;
  bool CloseOverflowMenuIfOpen() override;
  bool CanShowToolbarActionPopupForAPICall(
      const ToolbarActionsModel::ActionId& action_id) override;
  void ToggleExtensionsMenu() override;

  // ExtensionsToolbarViewModel::Observer:
  void OnActionsInitialized() override;
  void OnActionAdded(const ToolbarActionsModel::ActionId& action_id) override;
  void OnActionRemoved(const ToolbarActionsModel::ActionId& action_id) override;
  void OnActionUpdated(const ToolbarActionsModel::ActionId& action_id) override;
  void OnPinnedActionsChanged() override;
  void OnActiveWebContentsChanged(bool is_same_document) override;
  void OnToolbarControlStateUpdated() override;
  void OnRequestAccessButtonParamsChanged(
      content::WebContents* web_contents) override;

  // JNI implementations.
  void Destroy(JNIEnv* env);
  base::android::ScopedJavaLocalRef<jobject> GetRequestAccessButtonParams(
      JNIEnv* env,
      content::WebContents* web_contents);
  base::android::ScopedJavaLocalRef<jobject> GetAction(
      JNIEnv* env,
      const ToolbarActionsModel::ActionId& action_id);
  base::android::ScopedJavaLocalRef<jobject> GetIcon(
      JNIEnv* env,
      const ToolbarActionsModel::ActionId& action_id,
      content::WebContents* web_contents,
      int canvas_width_dp,
      int canvas_height_dp,
      float scale_factor);
  std::vector<ToolbarActionsModel::ActionId> GetAllActionIds(JNIEnv* env);
  std::vector<ToolbarActionsModel::ActionId> GetPinnedActionIds(JNIEnv* env);
  int GetExtensionsMenuButtonState(JNIEnv* env,
                                   content::WebContents* web_contents);
  void ExecuteUserAction(const ToolbarActionsModel::ActionId& action_id,
                         ToolbarActionViewModel::InvocationSource source);
  void MovePinnedAction(const ToolbarActionsModel::ActionId& action_id,
                        int target_index);

 private:
  void RegisterIconObserverForAction(
      const ToolbarActionsModel::ActionId& action_id);

  void OnActionIconUpdated(const ToolbarActionsModel::ActionId& action_id);

  const raw_ptr<BrowserWindowInterface> browser_;

  // The view model for this container.
  std::unique_ptr<ExtensionsToolbarViewModel> toolbar_view_model_;

  // Registers ExtensionsToolbarViewModel as the ExtensionsContainer for the
  // browser window.
  ui::ScopedUnownedUserData<ExtensionsContainer>
      scoped_toolbar_view_model_user_data_;

  // Map of action IDs to their respective `ExtensionActionViewModel` update
  // subscriptions for icon updates.
  std::map<ToolbarActionsModel::ActionId, base::CallbackListSubscription>
      icon_subscriptions_;

  // Observes and listens to changes to the view model.
  base::ScopedObservation<ExtensionsToolbarViewModel,
                          ExtensionsToolbarViewModel::Observer>
      toolbar_view_model_observation_{this};

  const base::android::ScopedJavaGlobalRef<jobject> java_object_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_ANDROID_EXTENSIONS_EXTENSIONS_TOOLBAR_ANDROID_H_
