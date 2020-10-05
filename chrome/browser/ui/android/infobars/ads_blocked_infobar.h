// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_INFOBARS_ADS_BLOCKED_INFOBAR_H_
#define CHROME_BROWSER_UI_ANDROID_INFOBARS_ADS_BLOCKED_INFOBAR_H_

#include "base/macros.h"
#include "chrome/browser/ui/android/content_settings/ads_blocked_infobar_delegate.h"
#include "components/infobars/android/confirm_infobar.h"

class AdsBlockedInfoBar : public infobars::ConfirmInfoBar {
 public:
  AdsBlockedInfoBar(std::unique_ptr<AdsBlockedInfobarDelegate> delegate,
                    const ResourceIdMapper& resource_id_mapper);

  ~AdsBlockedInfoBar() override;

 private:
  // ConfirmInfoBar:
  base::android::ScopedJavaLocalRef<jobject> CreateRenderInfoBar(
      JNIEnv* env) override;

  DISALLOW_COPY_AND_ASSIGN(AdsBlockedInfoBar);
};

#endif  // CHROME_BROWSER_UI_ANDROID_INFOBARS_ADS_BLOCKED_INFOBAR_H_
