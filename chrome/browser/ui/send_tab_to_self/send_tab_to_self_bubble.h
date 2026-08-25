// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_BUBBLE_H_
#define CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_BUBBLE_H_

#include "components/send_tab_to_self/metrics_util.h"

namespace content {
class WebContents;
}  // namespace content

namespace send_tab_to_self {

void ShowBubble(content::WebContents* web_contents,
                ShareEntryPoint entry_point,
                bool show_back_button = false);

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_BUBBLE_H_
