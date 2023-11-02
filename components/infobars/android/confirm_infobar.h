// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INFOBARS_ANDROID_CONFIRM_INFOBAR_H_
#define COMPONENTS_INFOBARS_ANDROID_CONFIRM_INFOBAR_H_

#include <string>

#include "base/android/scoped_java_ref.h"
#include "components/infobars/android/infobar_android.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

namespace infobars {

class ConfirmInfoBar : public InfoBarAndroid {
 public:
  explicit ConfirmInfoBar(std::unique_ptr<ConfirmInfoBarDelegate> delegate);

  ConfirmInfoBar(const ConfirmInfoBar&) = delete;
  ConfirmInfoBar& operator=(const ConfirmInfoBar&) = delete;

  ~ConfirmInfoBar() override;

 protected:
  ConfirmInfoBarDelegate* GetDelegate();
  std::u16string GetTextFor(ConfirmInfoBarDelegate::InfoBarButton button);

  // InfoBarAndroid overrides.
  base::android::ScopedJavaLocalRef<jobject> CreateRenderInfoBar(
      JNIEnv* env,
      const ResourceIdMapper& resource_id_mapper) override;

  void OnLinkClicked(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj) override;

  void ProcessButton(int action) override;
};

}  // namespace infobars

#endif  // COMPONENTS_INFOBARS_ANDROID_CONFIRM_INFOBAR_H_
