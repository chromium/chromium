// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_EXTENSIONS_EXTENSION_ACTIONS_BRIDGE_H_
#define CHROME_BROWSER_UI_ANDROID_EXTENSIONS_EXTENSION_ACTIONS_BRIDGE_H_

#include <memory>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/android/extensions/extension_keybinding_registry_android.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/extension_action_icon_factory.h"
#include "third_party/jni_zero/jni_zero.h"

namespace extensions {

class ExtensionActionsBridge : public ToolbarActionsModel::Observer,
                               public KeyedService {
 public:
  explicit ExtensionActionsBridge(Profile* profile);
  ExtensionActionsBridge(const ExtensionActionsBridge&) = delete;
  ExtensionActionsBridge& operator=(const ExtensionActionsBridge&) = delete;
  ~ExtensionActionsBridge() override;

  // Convenience function to get the ExtensionActionsBridge for a Profile.
  static ExtensionActionsBridge* Get(Profile* profile);

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  // JNI implementations.
  bool AreActionsInitialized(JNIEnv* env);
  std::vector<ToolbarActionsModel::ActionId> GetActionIds(JNIEnv* env);
  base::android::ScopedJavaLocalRef<jobject> GetAction(
      JNIEnv* env,
      const ToolbarActionsModel::ActionId& action_id,
      int tab_id);
  base::android::ScopedJavaLocalRef<jobject> GetActionIcon(
      JNIEnv* env,
      const ToolbarActionsModel::ActionId& action_id,
      int tab_id,
      const content::WebContents* web_contents,
      int canvas_width_dp,
      int canvas_height_dp,
      float scale_factor);
  ExtensionAction::ShowAction RunAction(
      JNIEnv* env,
      const ToolbarActionsModel::ActionId& action_id,
      int tab_id,
      content::WebContents* web_contents);
  jni_zero::ScopedJavaLocalRef<jobject> HandleKeyDownEvent(
      JNIEnv* env,
      const ui::KeyEventAndroid& key_event);

  // ToolbarActionsModel::Observer:
  void OnToolbarActionAdded(const ToolbarActionsModel::ActionId& id) override;
  void OnToolbarActionRemoved(const ToolbarActionsModel::ActionId& id) override;
  void OnToolbarActionUpdated(const ToolbarActionsModel::ActionId& id) override;
  void OnToolbarModelInitialized() override;
  void OnToolbarPinnedActionsChanged() override;

 private:
  // The observer of the icon of a single action. It is owned by
  // ExtensionActionBridge.
  class IconObserver : public extensions::ExtensionActionIconFactory::Observer {
   public:
    IconObserver(ExtensionActionsBridge* bridge,
                 const extensions::Extension& extension,
                 extensions::ExtensionAction& action);
    IconObserver(const IconObserver&) = delete;
    IconObserver& operator=(const IconObserver&) = delete;
    ~IconObserver() override;

    // Returns the current icon of the action.
    gfx::Image GetIcon(int tab_id);

    // extensions::ExtensionActionIconFactory::Observer:
    void OnIconUpdated() override;

   private:
    raw_ptr<ExtensionActionsBridge> bridge_;
    ToolbarActionsModel::ActionId action_id_;
    extensions::ExtensionActionIconFactory icon_factory_;
  };

  // Creates an IconObserver for an action if it does not exist. Otherwise, it
  // returns the cached instance. It returns nullptr for invalid action IDs.
  IconObserver* EnsureIconObserver(
      const ToolbarActionsModel::ActionId& action_id);

  // Removes a cached IconObserver for an action if it exists. Otherwise, it
  // does nothing.
  void RemoveIconObserver(const ToolbarActionsModel::ActionId& action_id);

  // Called by IconObserver to notify that the icon of an action was updated.
  void OnToolbarIconUpdated(const ToolbarActionsModel::ActionId& action_id);

  raw_ptr<Profile> profile_;
  raw_ptr<ToolbarActionsModel> model_;
  std::unique_ptr<ExtensionKeybindingRegistryAndroid> keybinding_registry_;
  std::map<ToolbarActionsModel::ActionId, std::unique_ptr<IconObserver>>
      icon_observers_;
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
  base::ScopedObservation<ToolbarActionsModel, ToolbarActionsModel::Observer>
      model_observation_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_ANDROID_EXTENSIONS_EXTENSION_ACTIONS_BRIDGE_H_
