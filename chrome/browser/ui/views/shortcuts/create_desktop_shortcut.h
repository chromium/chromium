// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SHORTCUTS_CREATE_DESKTOP_SHORTCUT_H_
#define CHROME_BROWSER_UI_VIEWS_SHORTCUTS_CREATE_DESKTOP_SHORTCUT_H_

#include <string>

#include "base/functional/callback_forward.h"

namespace content {
class WebContents;
}  // namespace content

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace chrome {

// Used to return the following information from the Create Desktop Shortcut
// dialog:
// 1. A boolean indicating if the dialog was accepted or cancelled. Cancellation
// covers both manual as well as automatic cancellations of the dialog.
// 2. The title that should be used for the shortcut name.
using CreateShortcutDialogCallback =
    base::OnceCallback<void(bool /*is_accepted*/, std::u16string /*title*/)>;

// Public accessor to ShowCreateDesktopShortcutDialog() for testing.
void ShowCreateDesktopShortcutDialogForTesting(
    content::WebContents* web_contents,
    const gfx::ImageSkia& icon,
    std::u16string title,
    CreateShortcutDialogCallback dialog_action_and_text_callback);

// Utility function that triggers a CreateShortcutForCurrentWebContentsTask to
// start the shortcut creation flow.
void CreateShortcutForWebContents(
    content::WebContents* web_contents,
    base::OnceCallback<void(bool shortcuts_created)>
        shortcut_creation_callback);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_VIEWS_SHORTCUTS_CREATE_DESKTOP_SHORTCUT_H_
