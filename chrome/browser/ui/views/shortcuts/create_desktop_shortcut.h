// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SHORTCUTS_CREATE_DESKTOP_SHORTCUT_H_
#define CHROME_BROWSER_UI_VIEWS_SHORTCUTS_CREATE_DESKTOP_SHORTCUT_H_

#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "ui/base/interaction/element_tracker.h"

namespace content {
class WebContents;
}  // namespace content

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace shortcuts {

// Callback that returns the information in the textfield of the create desktop
// shortcut dialog if accepted, or std::nullopt otherwise.
using CreateShortcutDialogCallback =
    base::OnceCallback<void(std::optional<std::u16string>)>;

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

}  // namespace shortcuts

#endif  // CHROME_BROWSER_UI_VIEWS_SHORTCUTS_CREATE_DESKTOP_SHORTCUT_H_
