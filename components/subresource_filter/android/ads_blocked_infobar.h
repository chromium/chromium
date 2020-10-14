// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_ANDROID_ADS_BLOCKED_INFOBAR_H_
#define COMPONENTS_SUBRESOURCE_FILTER_ANDROID_ADS_BLOCKED_INFOBAR_H_

#include "base/macros.h"
#include "components/infobars/android/confirm_infobar.h"
#include "components/subresource_filter/android/ads_blocked_infobar_delegate.h"

namespace subresource_filter {

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

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_ANDROID_ADS_BLOCKED_INFOBAR_H_
