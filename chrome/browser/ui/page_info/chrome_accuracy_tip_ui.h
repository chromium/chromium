// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PAGE_INFO_CHROME_ACCURACY_TIP_UI_H_
#define CHROME_BROWSER_UI_PAGE_INFO_CHROME_ACCURACY_TIP_UI_H_

#include "components/accuracy_tips/accuracy_tip_status.h"
#include "components/accuracy_tips/accuracy_tip_ui.h"

class ChromeAccuracyTipUI : public accuracy_tips::AccuracyTipUI {
 public:
  void ShowAccuracyTip(
      content::WebContents* web_contents,
      accuracy_tips::AccuracyTipStatus type,
      base::OnceCallback<void(Interaction)> close_callback) override;
};

// Definition for platform specific view implementation.
void ShowAccuracyTipDialog(
    content::WebContents* web_contents,
    accuracy_tips::AccuracyTipStatus type,
    base::OnceCallback<void(accuracy_tips::AccuracyTipUI::Interaction)>
        close_callback);

#endif  // CHROME_BROWSER_UI_PAGE_INFO_CHROME_ACCURACY_TIP_UI_H_
