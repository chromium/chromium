// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_ANDROID_INSTALLABLE_INSTALLABLE_AMBIENT_BADGE_INFOBAR_H_
#define COMPONENTS_WEBAPPS_BROWSER_ANDROID_INSTALLABLE_INSTALLABLE_AMBIENT_BADGE_INFOBAR_H_

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "components/infobars/android/infobar_android.h"

namespace webapps {

class InstallableAmbientBadgeInfoBarDelegate;

// An infobar shown to users when they visit a progressive web app.
class InstallableAmbientBadgeInfoBar : public infobars::InfoBarAndroid {
 public:
  explicit InstallableAmbientBadgeInfoBar(
      std::unique_ptr<InstallableAmbientBadgeInfoBarDelegate> delegate);

  InstallableAmbientBadgeInfoBar(const InstallableAmbientBadgeInfoBar&) =
      delete;
  InstallableAmbientBadgeInfoBar& operator=(
      const InstallableAmbientBadgeInfoBar&) = delete;

  ~InstallableAmbientBadgeInfoBar() override;

  void AddToHomescreen(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& obj);

 private:
  InstallableAmbientBadgeInfoBarDelegate* GetDelegate();

  // infobars::InfoBarAndroid:
  base::android::ScopedJavaLocalRef<jobject> CreateRenderInfoBar(
      JNIEnv* env,
      const ResourceIdMapper& resource_id_mapper) override;
  void ProcessButton(int action) override;
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_ANDROID_INSTALLABLE_INSTALLABLE_AMBIENT_BADGE_INFOBAR_H_
