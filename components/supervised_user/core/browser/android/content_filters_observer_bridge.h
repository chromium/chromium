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
#include "base/memory/raw_ref.h"
#include "base/observer_list.h"

// Required to veto Android Parental Control changes if the user is already
// subject to parental controls.
class PrefService;

namespace supervised_user {
// Forward-declared to go around dependency-cycle.
bool IsSubjectToParentalControls(const PrefService& pref_service);

// Bridge between the C++ and Java sides for a content filters observer. Used
// to observe the Android's secure settings, can be a component of a service.
// observer. Instances of FakeContentFiltersObserverBridge for testing
// purposes are available from SupervisedUserTestEnvironment.
class ContentFiltersObserverBridge {
 public:
  // Observers will receive notifications about changes to underlying settings
  // storage.
  class Observer {
   public:
    virtual void OnContentFiltersObserverEnabled(
        std::string_view setting_name) {}
    virtual void OnContentFiltersObserverDisabled(
        std::string_view setting_name) {}
  };

  // Creates a bridge for a given setting. If present, the pref service is used
  // to veto Android Parental Control changes if the user is already subject to
  // parental controls.
  ContentFiltersObserverBridge(std::string_view setting_name,
                               const PrefService* pref_service);

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

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void SetEnabledForTesting(bool enabled);

  base::WeakPtr<ContentFiltersObserverBridge> GetWeakPtr();

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
  raw_ptr<const PrefService> pref_service_;
  base::android::ScopedJavaGlobalRef<jobject> bridge_;
  base::ObserverList<Observer>::Unchecked observer_list_;
  base::WeakPtrFactory<ContentFiltersObserverBridge> weak_ptr_factory_{this};
};
}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_ANDROID_CONTENT_FILTERS_OBSERVER_BRIDGE_H_
