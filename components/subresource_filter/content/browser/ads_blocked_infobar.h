// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_ADS_BLOCKED_INFOBAR_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_ADS_BLOCKED_INFOBAR_H_

#include "components/infobars/android/confirm_infobar.h"
#include "components/subresource_filter/content/browser/ads_blocked_infobar_delegate.h"

namespace subresource_filter {

class AdsBlockedInfoBar : public infobars::ConfirmInfoBar {
 public:
  explicit AdsBlockedInfoBar(
      std::unique_ptr<AdsBlockedInfobarDelegate> delegate);

  AdsBlockedInfoBar(const AdsBlockedInfoBar&) = delete;
  AdsBlockedInfoBar& operator=(const AdsBlockedInfoBar&) = delete;

  ~AdsBlockedInfoBar() override;

 private:
  // ConfirmInfoBar:
  base::android::ScopedJavaLocalRef<jobject> CreateRenderInfoBar(
      JNIEnv* env,
      const ResourceIdMapper& resource_id_mapper) override;
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_ADS_BLOCKED_INFOBAR_H_
