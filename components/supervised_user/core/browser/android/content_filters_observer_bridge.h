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
#include "base/observer_list.h"

namespace supervised_user {

// Bridge between the C++ and Java sides for a content filters observer. Used
// to observe the Android's secure settings. See AndroidParentalControls for a
// container with known secure settings. SupervisedUserTestEnvironment or
// TestingBrowserProcess are the recommended ways to interact with instances of
// this class for testing.
class ContentFiltersObserverBridge {
 public:
  // Observers will receive notifications about changes to underlying settings
  // storage.
  class Observer {
   public:
    virtual void OnContentFiltersObserverChanged() {}
    virtual ~Observer() = default;
  };

  // Creates a bridge that will observe the given setting.
  explicit ContentFiltersObserverBridge(std::string_view setting_name);

  ContentFiltersObserverBridge(const ContentFiltersObserverBridge&) = delete;
  ContentFiltersObserverBridge& operator=(const ContentFiltersObserverBridge&) =
      delete;
  virtual ~ContentFiltersObserverBridge();

  // Initializes and shuts down the java class.
  virtual void Init();
  virtual void Shutdown();

  // Called after creating the bridge and when the setting is enabled or
  // disabled. Triggers observers.
  void OnChange(JNIEnv* env, bool enabled);

  // Reads the last broadcasted value of the setting from the Java side.
  bool IsEnabled() const;

  // Observer registration mutates this instance, but it is not altering its
  // internal logical state.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void SetEnabledForTesting(bool enabled);

  std::string_view GetSettingName() const { return setting_name_; }

 protected:
  void SetEnabled(bool enabled);

  // Notifies observers about the current state of the setting.
  void NotifyObservers();

 private:
  // In prod environment set from Java via JNI and reflects the current state
  // of the setting in the operating system. In test environment that removes
  // Java native storage backend, it might be controlled explicitly.
  bool enabled_ = false;
  std::string setting_name_;
  base::android::ScopedJavaGlobalRef<jobject> bridge_;
  // Observer list is mutable to allow adding observers even when the referenced
  // "this instance" is const (e.g. when used in KeyedServiceFactory).
  base::ObserverList<Observer>::Unchecked observer_list_;
};
}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_ANDROID_CONTENT_FILTERS_OBSERVER_BRIDGE_H_
