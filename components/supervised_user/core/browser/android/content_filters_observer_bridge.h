// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_ANDROID_CONTENT_FILTERS_OBSERVER_BRIDGE_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_ANDROID_CONTENT_FILTERS_OBSERVER_BRIDGE_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"

namespace supervised_user {
// Bridge between the C++ and Java sides for a content filters observer. Used to
// observe the Android's secure settings, can be a component of a service.
// observer. Instances of FakeContentFiltersObserverBridge for testing purposes
// are available from SupervisedUserTestEnvironment.
class ContentFiltersObserverBridge {
 public:
  // Factory for creating ContentFiltersObserverBridge instances. They should
  // accept the setting name, two callbacks to be called when the setting is
  // enabled or disabled and the user prefs.
  using Factory =
      base::RepeatingCallback<std::unique_ptr<ContentFiltersObserverBridge>(
          std::string_view,
          base::RepeatingClosure,
          base::RepeatingClosure,
          base::RepeatingCallback<bool()>)>;

  // Creates a ContentFiltersObserverBridge instance.
  static std::unique_ptr<ContentFiltersObserverBridge> Create(
      std::string_view setting_name,
      base::RepeatingClosure on_enabled,
      base::RepeatingClosure on_disabled,
      base::RepeatingCallback<bool()> is_subject_to_parental_controls);

  ContentFiltersObserverBridge(
      std::string_view setting_name,
      base::RepeatingClosure on_enabled,
      base::RepeatingClosure on_disabled,
      base::RepeatingCallback<bool()> is_subject_to_parental_controls);

  ContentFiltersObserverBridge(const ContentFiltersObserverBridge&) = delete;
  ContentFiltersObserverBridge& operator=(const ContentFiltersObserverBridge&) =
      delete;
  virtual ~ContentFiltersObserverBridge();

  // Initializes and shuts down the java class.
  virtual void Init();
  virtual void Shutdown();

  // Called after creating the bridge and when the setting is enabled or
  // disabled.
  void OnChange(JNIEnv* env, bool enabled);

  // Reads the last broadcasted value of the setting from the Java side.
  bool IsEnabled() const;

 protected:
  virtual void SetEnabled(bool enabled);

 private:
  // This value is set exclusively from Java, and reflects the last broadcasted
  // value of the setting.
  bool enabled_ = false;
  std::string setting_name_;
  base::RepeatingClosure on_enabled_;
  base::RepeatingClosure on_disabled_;
  base::RepeatingCallback<bool()> is_subject_to_parental_controls_;
  base::android::ScopedJavaGlobalRef<jobject> bridge_;
};
}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_ANDROID_CONTENT_FILTERS_OBSERVER_BRIDGE_H_
