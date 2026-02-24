// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/extensions/extensions_menu_view_model.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_handler.h"
#include "third_party/skia/include/core/SkBitmap.h"

#ifndef CHROME_BROWSER_UI_ANDROID_EXTENSIONS_EXTENSIONS_MENU_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_EXTENSIONS_EXTENSIONS_MENU_DELEGATE_ANDROID_H_

namespace extensions {

// Implements Android-specific extensions menu UI logic. Acts as the JNI bridge
// for the extensions menu. This bridge is created and owned by Java UI code.
class ExtensionsMenuDelegateAndroid : public ExtensionsMenuViewModel::Delegate,
                                      public ExtensionsMenuViewModel::Observer,
                                      public ExtensionsMenuHandler {
 public:
  ExtensionsMenuDelegateAndroid(
      BrowserWindowInterface* browser,
      const base::android::JavaRef<jobject>& java_object);
  ExtensionsMenuDelegateAndroid(const ExtensionsMenuDelegateAndroid&) = delete;
  const ExtensionsMenuDelegateAndroid& operator=(
      const ExtensionsMenuDelegateAndroid&) = delete;
  ~ExtensionsMenuDelegateAndroid() override;

  // JNI implementations:
  void Destroy(JNIEnv* env);
  base::android::ScopedJavaLocalRef<jobject> GetActionIcon(JNIEnv* env,
                                                           int action_index);
  std::vector<base::android::ScopedJavaLocalRef<jobject>> GetMenuEntries(
      JNIEnv* env);
  base::android::ScopedJavaLocalRef<jobject> GetSiteSettings(JNIEnv* env);
  bool IsReady(JNIEnv* env);
  void OnSiteSettingsToggleChanged(JNIEnv* env, bool is_checked);

  // ExtensionsMenuViewModel::Delegate:
  std::unique_ptr<ExtensionActionViewModel> CreateActionViewModel(
      const extensions::ExtensionId& extension_id) override;

  // ExtensionsMenuViewModel::Observer:
  void OnPageNavigation() override;
  void OnActionAdded(ExtensionActionViewModel* action_model,
                     int index) override;
  void OnActionRemoved(const ToolbarActionsModel::ActionId& action_id,
                       int index) override;
  void OnActionUpdated(const ToolbarActionsModel::ActionId& action_id) override;
  void OnActionsInitialized() override;
  void OnHostAccessRequestAdded(const extensions::ExtensionId& extension_id,
                                int index) override;
  void OnHostAccessRequestUpdated(const extensions::ExtensionId& extension_id,
                                  int index) override;
  void OnActionIconUpdated(
      const ToolbarActionsModel::ActionId& action_id) override;
  void OnHostAccessRequestsCleared() override;
  void OnHostAccessRequestRemoved(const extensions::ExtensionId& extension_id,
                                  int index) override;
  void OnShowHostAccessRequestsInToolbarChanged(
      const extensions::ExtensionId& extension_id,
      bool can_show_requests) override;
  void OnToolbarPinnedActionsChanged() override;
  void OnUserPermissionsSettingsChanged() override;

  // ExtensionsMenuHandler:
  void CloseBubble() override;
  void OnActionButtonClicked(
      const extensions::ExtensionId& extension_id) override;
  void OnAllowExtensionClicked(
      const extensions::ExtensionId& extension_id) override;
  void OnDismissExtensionClicked(
      const extensions::ExtensionId& extension_id) override;
  void OnExtensionToggleSelected(const extensions::ExtensionId& extension_id,
                                 bool is_on) override;
  void OnReloadPageButtonClicked() override;
  void OnShowRequestsTogglePressed(const extensions::ExtensionId& extension_id,
                                   bool is_on) override;
  void OnSiteAccessSelected(
      const extensions::ExtensionId& extension_id,
      extensions::PermissionsManager::UserSiteAccess site_access) override;
  void OnSiteSettingsToggleButtonPressed(bool is_on) override;
  void OpenMainPage() override;
  void OpenSitePermissionsPage(
      const extensions::ExtensionId& extension_id) override;

 private:
  // Notifies the Java side that the menu is ready to be shown.
  void OnReady();

  const raw_ptr<BrowserWindowInterface> browser_;

  // The platform-agnostic menu view model.
  std::unique_ptr<ExtensionsMenuViewModel> menu_model_;
  base::ScopedObservation<ExtensionsMenuViewModel,
                          ExtensionsMenuViewModel::Observer>
      menu_model_observation_{this};

  const base::android::ScopedJavaGlobalRef<jobject> java_object_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_ANDROID_EXTENSIONS_EXTENSIONS_MENU_DELEGATE_ANDROID_H_
