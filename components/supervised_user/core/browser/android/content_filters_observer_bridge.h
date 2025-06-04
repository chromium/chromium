// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_ANDROID_CONTENT_FILTERS_OBSERVER_BRIDGE_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_ANDROID_CONTENT_FILTERS_OBSERVER_BRIDGE_H_

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"

namespace supervised_user {

// Bridge between the C++ and Java sides for a content filters observer. Used to
// observe the Android's secure settings, can be a component of a service.
// observer.
class ContentFiltersObserverBridge {
 public:
  // The `setting_name` is the name of the Android's secure setting to observe.
  // The `on_enabled` and `on_disabled` closures are called when the setting is
  // enabled or disabled.
  explicit ContentFiltersObserverBridge(std::string_view setting_name,
                                        base::RepeatingClosure on_enabled,
                                        base::RepeatingClosure on_disabled);
  ~ContentFiltersObserverBridge();

  // Called after creating the bridge and when the setting is enabled or
  // disabled.
  void OnChange(JNIEnv* env, bool enabled);

 private:
  base::android::ScopedJavaGlobalRef<jobject> bridge_;
  base::RepeatingClosure on_enabled_;
  base::RepeatingClosure on_disabled_;
};

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_ANDROID_CONTENT_FILTERS_OBSERVER_BRIDGE_H_
