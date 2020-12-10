// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/chrome_confirm_infobar.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "content/public/browser/web_contents.h"

// InfoBarService -------------------------------------------------------------

std::unique_ptr<infobars::InfoBar> InfoBarService::CreateConfirmInfoBar(
    std::unique_ptr<ConfirmInfoBarDelegate> delegate) {
  return std::make_unique<ChromeConfirmInfoBar>(std::move(delegate));
}

// ChromeConfirmInfoBar
// -------------------------------------------------------------

ChromeConfirmInfoBar::ChromeConfirmInfoBar(
    std::unique_ptr<ConfirmInfoBarDelegate> delegate)
    : infobars::ConfirmInfoBar(std::move(delegate)) {}

ChromeConfirmInfoBar::~ChromeConfirmInfoBar() = default;

TabAndroid* ChromeConfirmInfoBar::GetTab() {
  content::WebContents* web_contents =
      InfoBarService::WebContentsFromInfoBar(this);
  DCHECK(web_contents);

  TabAndroid* tab = TabAndroid::FromWebContents(web_contents);
  DCHECK(tab);
  return tab;
}
