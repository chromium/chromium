// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_INFOBARS_CHROME_CONFIRM_INFOBAR_H_
#define CHROME_BROWSER_UI_ANDROID_INFOBARS_CHROME_CONFIRM_INFOBAR_H_

#include "components/infobars/android/confirm_infobar.h"

class TabAndroid;

// Chrome-specific convenience specialization of ConfirmInfoBar that supplies
// Chrome-level parameters.
class ChromeConfirmInfoBar : public infobars::ConfirmInfoBar {
 public:
  explicit ChromeConfirmInfoBar(
      std::unique_ptr<ConfirmInfoBarDelegate> delegate);
  ~ChromeConfirmInfoBar() override;

  ChromeConfirmInfoBar(const ChromeConfirmInfoBar&) = delete;
  ChromeConfirmInfoBar& operator=(const ChromeConfirmInfoBar&) = delete;

 protected:
  TabAndroid* GetTab();
};

#endif  // CHROME_BROWSER_UI_ANDROID_INFOBARS_CHROME_CONFIRM_INFOBAR_H_
