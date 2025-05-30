// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_TOOLBAR_EXTENSION_ACTIONS_BRIDGE_H_
#define CHROME_BROWSER_UI_ANDROID_TOOLBAR_EXTENSION_ACTIONS_BRIDGE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/extension_action_icon_factory.h"

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
  jboolean AreActionsInitialized(JNIEnv* env);
  std::vector<std::string> GetActionIds(JNIEnv* env);
  base::android::ScopedJavaLocalRef<jobject>
  GetAction(JNIEnv* env, const std::string& action_id, jint tab_id);
  base::android::ScopedJavaLocalRef<jobject>
  GetActionIcon(JNIEnv* env, const std::string& action_id, jint tab_id);
  jint RunAction(JNIEnv* env,
                 const std::string& action_id,
                 const base::android::JavaParamRef<jobject>& web_contents_java);

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
  std::map<ToolbarActionsModel::ActionId, std::unique_ptr<IconObserver>>
      icon_observers_;
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
  base::ScopedObservation<ToolbarActionsModel, ToolbarActionsModel::Observer>
      model_observation_{this};
};

#endif  // CHROME_BROWSER_UI_ANDROID_TOOLBAR_EXTENSION_ACTIONS_BRIDGE_H_
