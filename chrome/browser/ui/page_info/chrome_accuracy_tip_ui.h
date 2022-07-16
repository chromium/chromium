// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PAGE_INFO_CHROME_ACCURACY_TIP_UI_H_
#define CHROME_BROWSER_UI_PAGE_INFO_CHROME_ACCURACY_TIP_UI_H_

#include "base/callback_forward.h"
#include "components/accuracy_tips/accuracy_tip_interaction.h"
#include "components/accuracy_tips/accuracy_tip_status.h"

namespace content {
class WebContents;
}

// Definition for platform specific view implementation.
void ShowAccuracyTipDialog(
    content::WebContents* web_contents,
    accuracy_tips::AccuracyTipStatus type,
    bool show_opt_out,
    base::OnceCallback<void(accuracy_tips::AccuracyTipInteraction)>
        close_callback);

#endif  // CHROME_BROWSER_UI_PAGE_INFO_CHROME_ACCURACY_TIP_UI_H_
