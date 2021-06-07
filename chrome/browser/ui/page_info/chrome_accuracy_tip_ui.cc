// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/page_info/chrome_accuracy_tip_ui.h"

#include "base/callback.h"
#include "components/accuracy_tips/accuracy_tip_status.h"

void ChromeAccuracyTipUI::ShowAccuracyTip(
    content::WebContents* web_contents,
    accuracy_tips::AccuracyTipStatus status,
    base::OnceCallback<void(Interaction)> close_callback) {
  // TODO(crbug.com/1210891): Try to use DialogModel instead of custom view.
  ShowAccuracyTipDialog(web_contents, status, std::move(close_callback));
}
