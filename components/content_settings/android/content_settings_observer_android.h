// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_ANDROID_CONTENT_SETTINGS_OBSERVER_ANDROID_H_
#define COMPONENTS_CONTENT_SETTINGS_ANDROID_CONTENT_SETTINGS_OBSERVER_ANDROID_H_

#include "base/android/jni_weak_ref.h"
#include "base/scoped_observation.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"

namespace content_settings {

class AndroidObserver : public Observer {
 public:
  AndroidObserver(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& jbrowser_context_handle);

  // Destroys the AndroidObserver object. This needs to be called on the java
  // side when the object is not in use anymore.
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  // content_settings::Observer:
  void OnContentSettingChanged(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsTypeSet content_type_set) override;

 protected:
  ~AndroidObserver() override;

 private:
  base::android::ScopedJavaGlobalRef<jobject> jobject_;
  base::ScopedObservation<HostContentSettingsMap, content_settings::Observer>
      content_settings_observation_{this};
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_ANDROID_CONTENT_SETTINGS_OBSERVER_ANDROID_H_
