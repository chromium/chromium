// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_TOOLBAR_TOOLBAR_ACTIONS_BRIDGE_H_
#define CHROME_BROWSER_UI_ANDROID_TOOLBAR_TOOLBAR_ACTIONS_BRIDGE_H_

#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "components/keyed_service/core/keyed_service.h"

class ToolbarActionsBridge : public ToolbarActionsModel::Observer,
                             public KeyedService {
 public:
  explicit ToolbarActionsBridge(Profile* profile);
  ToolbarActionsBridge(const ToolbarActionsBridge&) = delete;
  ToolbarActionsBridge& operator=(const ToolbarActionsBridge&) = delete;
  ~ToolbarActionsBridge() override;

  // Convenience function to get the ToolbarActionsBridge for a Profile.
  static ToolbarActionsBridge* Get(Profile* profile);

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  // JNI implementations.
  jboolean AreActionsInitialized(JNIEnv* env);
  std::vector<std::string> GetActionIds(JNIEnv* env);
  base::android::ScopedJavaLocalRef<jobject>
  GetAction(JNIEnv* env, const std::string& action_id, jint tab_id);

  // ToolbarActionsModel::Observer:
  void OnToolbarActionAdded(const ToolbarActionsModel::ActionId& id) override;
  void OnToolbarActionRemoved(const ToolbarActionsModel::ActionId& id) override;
  void OnToolbarActionUpdated(const ToolbarActionsModel::ActionId& id) override;
  void OnToolbarModelInitialized() override;
  void OnToolbarPinnedActionsChanged() override;

 private:
  raw_ptr<Profile> profile_;
  raw_ptr<ToolbarActionsModel> model_;
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
  base::ScopedObservation<ToolbarActionsModel, ToolbarActionsModel::Observer>
      model_observation_{this};
};

#endif  // CHROME_BROWSER_UI_ANDROID_TOOLBAR_TOOLBAR_ACTIONS_BRIDGE_H_
