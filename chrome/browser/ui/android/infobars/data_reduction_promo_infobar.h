// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_INFOBARS_DATA_REDUCTION_PROMO_INFOBAR_H_
#define CHROME_BROWSER_UI_ANDROID_INFOBARS_DATA_REDUCTION_PROMO_INFOBAR_H_

#include "chrome/browser/data_reduction_proxy/data_reduction_promo_infobar_delegate_android.h"
#include "components/infobars/android/confirm_infobar.h"

class DataReductionPromoInfoBar : public infobars::ConfirmInfoBar {
 public:
  explicit DataReductionPromoInfoBar(
      std::unique_ptr<DataReductionPromoInfoBarDelegateAndroid> delegate);

  DataReductionPromoInfoBar(const DataReductionPromoInfoBar&) = delete;
  DataReductionPromoInfoBar& operator=(const DataReductionPromoInfoBar&) =
      delete;

  ~DataReductionPromoInfoBar() override;

 private:
  // ConfirmInfoBar:
  base::android::ScopedJavaLocalRef<jobject> CreateRenderInfoBar(
      JNIEnv* env,
      const ResourceIdMapper& resource_id_mapper) override;

  DataReductionPromoInfoBarDelegateAndroid* GetDelegate();
};

#endif  // CHROME_BROWSER_UI_ANDROID_INFOBARS_DATA_REDUCTION_PROMO_INFOBAR_H_
