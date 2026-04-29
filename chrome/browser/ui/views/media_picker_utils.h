// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_PICKER_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_PICKER_UTILS_H_

#include "ui/gfx/native_ui_types.h"

class BrowserWindowInterface;

namespace content {
class WebContents;
}

namespace views {
class DialogDelegate;
class Widget;
}  // namespace views

// Returns whether the media picker dialog can show as a web modal window.
// If the picker was spawned from an invisible page (e.g. an extension
// background page) or there is no associated tab (e.g. an extension popup menu)
// then returns false.
bool MediaPickerCanShowAsWebModal(content::WebContents* web_contents);

// Creates a dialog and hides an extension popup if present.
// If `web_contents` is not a background page then the dialog will be shown
// modal to the `web_contents`. Otherwise, the dialog is shown in a separate
// window.
views::Widget* CreateMediaPickerDialogWidget(BrowserWindowInterface* browser,
                                             content::WebContents* web_contents,
                                             views::DialogDelegate* delegate,
                                             gfx::NativeWindow context,
                                             gfx::NativeView parent);

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_PICKER_UTILS_H_
