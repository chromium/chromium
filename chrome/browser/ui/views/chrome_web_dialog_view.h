// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CHROME_WEB_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_CHROME_WEB_DIALOG_VIEW_H_

#include "base/optional.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/widget/widget.h"

namespace content {
class BrowserContext;
}

namespace ui {
class WebDialogDelegate;
}

namespace chrome {

// Views specific implementation allows extra InitParams.
gfx::NativeWindow ShowWebDialogWithParams(
    gfx::NativeView parent,
    content::BrowserContext* context,
    ui::WebDialogDelegate* delegate,
    base::Optional<views::Widget::InitParams> extra_params);

// The implementation is more aligned with the appearance of constrained
// web dialog.
// |show| indicates whether to show the web dialog after it is created.
// TODO(weili): Solely use this function on non-ChromeOS platform, and
// above ShowWebDialogWithParams() on ChromeOS. Or merge these two if possible.
gfx::NativeWindow CreateWebDialogWithBounds(gfx::NativeView parent,
                                            content::BrowserContext* context,
                                            ui::WebDialogDelegate* delegate,
                                            const gfx::Rect& bounds,
                                            bool show = true);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_VIEWS_CHROME_WEB_DIALOG_VIEW_H_
