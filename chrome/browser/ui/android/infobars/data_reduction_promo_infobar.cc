// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/data_reduction_promo_infobar.h"

#include <memory>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_promo_infobar_delegate_android.h"
#include "content/public/browser/web_contents.h"

// DataReductionPromoInfoBar --------------------------------------------------

DataReductionPromoInfoBar::DataReductionPromoInfoBar(
    std::unique_ptr<DataReductionPromoInfoBarDelegateAndroid> delegate)
    : ChromeConfirmInfoBar(std::move(delegate)) {}

DataReductionPromoInfoBar::~DataReductionPromoInfoBar() {
}

base::android::ScopedJavaLocalRef<jobject>
DataReductionPromoInfoBar::CreateRenderInfoBar(
    JNIEnv* env,
    const ResourceIdMapper& resource_id_mapper) {
  return GetDelegate()->CreateRenderInfoBar(env);
}

DataReductionPromoInfoBarDelegateAndroid*
DataReductionPromoInfoBar::GetDelegate() {
  return static_cast<DataReductionPromoInfoBarDelegateAndroid*>(delegate());
}

// DataReductionPromoInfoBarDelegate ------------------------------------------

// static
std::unique_ptr<infobars::InfoBar>
DataReductionPromoInfoBarDelegateAndroid::CreateInfoBar(
    infobars::InfoBarManager* infobar_manager,
    std::unique_ptr<DataReductionPromoInfoBarDelegateAndroid> delegate) {
  return std::make_unique<DataReductionPromoInfoBar>(std::move(delegate));
}
