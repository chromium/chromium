// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_IDLE_BUBBLE_H_
#define CHROME_BROWSER_UI_IDLE_BUBBLE_H_

#include "base/functional/callback.h"
#include "chrome/browser/ui/idle_dialog.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_frame_view.h"

class Browser;

// Show a bubble informing the user that IdleTimeoutActions just ran.
void ShowIdleBubble(Browser* browser,
                    base::TimeDelta threshold,
                    IdleDialog::ActionSet actions,
                    base::OnceClosure on_close);

// TODO(crbug.com/40266853): Convert idle_service_browsertest.cc to an
// InteractiveTest, and get rid of this.
views::BubbleFrameView* GetIdleBubble(Browser* browser);

#endif  // CHROME_BROWSER_UI_IDLE_BUBBLE_H_
