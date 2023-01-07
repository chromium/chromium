// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_DIALOG_H_
#define CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_DIALOG_H_

#include "base/functional/callback.h"
#include "chrome/browser/ui/bubble_anchor_util.h"
#include "components/security_state/core/security_state.h"
#include "ui/views/widget/widget.h"

namespace content {
class WebContents;
}

class GURL;
class Browser;

// Callback that happens when the user closes the Page Info UI.
// The second parameter is whether closing the UI caused a reload prompt to be
// displayed to the user.
using PageInfoClosingCallback =
    base::OnceCallback<void(views::Widget::ClosedReason,
                            bool /* reload_prompt */)>;

// Shows PageInfo for the given |web_contents| in its browser. Returns false if
// the URL or parent Browser* can not be determined.
bool ShowPageInfoDialog(
    content::WebContents* web_contents,
    PageInfoClosingCallback closing_callback,
    bubble_anchor_util::Anchor = bubble_anchor_util::kLocationBar);

// Shows Page Info using the specified information. `virtual_url` is the virtual
// url of the page/frame the info applies to, and `security_level`,
// `visible_security_state` contain the security state for that page/frame.
// Implemented in platform-specific files. Before the cookie count can be
// displayed, the set of ignored empty storage keys must be updated. This
// happens asynchronously and `initialized_callback` fires after it has
// finished.
void ShowPageInfoDialogImpl(Browser* browser,
                            content::WebContents* web_contents,
                            const GURL& virtual_url,
                            bubble_anchor_util::Anchor,
                            base::OnceClosure initialized_callback,
                            PageInfoClosingCallback closing_callback);

// Gets the callback to run after a dialog is created. Only used in tests.
base::OnceClosure& GetPageInfoDialogCreatedCallbackForTesting();

#endif  // CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_DIALOG_H_
