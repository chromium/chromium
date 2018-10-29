// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_INFOBARS_PREVIEWS_LITE_PAGE_INFOBAR_H_
#define CHROME_BROWSER_UI_ANDROID_INFOBARS_PREVIEWS_LITE_PAGE_INFOBAR_H_

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "chrome/browser/previews/previews_lite_page_infobar_delegate.h"
#include "chrome/browser/ui/android/infobars/confirm_infobar.h"

// This InfoBar notifies the user that Data Saver now also applies to HTTPS
// pages.
class PreviewsLitePageInfoBar : public ConfirmInfoBar {
 public:
  explicit PreviewsLitePageInfoBar(
      std::unique_ptr<PreviewsLitePageInfoBarDelegate> delegate);

  ~PreviewsLitePageInfoBar() override;

  // Returns a Previews Lite page InfoBar that owns |delegate|.
  static std::unique_ptr<infobars::InfoBar> CreateInfoBar(
      infobars::InfoBarManager* infobar_manager,
      std::unique_ptr<PreviewsLitePageInfoBarDelegate> delegate);

 private:
  // ConfirmInfoBar:
  base::android::ScopedJavaLocalRef<jobject> CreateRenderInfoBar(
      JNIEnv* env) override;

  DISALLOW_COPY_AND_ASSIGN(PreviewsLitePageInfoBar);
};

#endif  // CHROME_BROWSER_UI_ANDROID_INFOBARS_PREVIEWS_LITE_PAGE_INFOBAR_H_
