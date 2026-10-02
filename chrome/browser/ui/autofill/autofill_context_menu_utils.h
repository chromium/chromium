// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_CONTEXT_MENU_UTILS_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_CONTEXT_MENU_UTILS_H_

namespace content {
struct ContextMenuParams;
class RenderFrameHost;
}  // namespace content

namespace autofill {

// Returns true if `params` corresponds to an editable text field (<input>,
// <textarea>, or contenteditable) where Autofill context menu items can appear.
bool ShouldShowAutofillContextMenu(const content::ContextMenuParams& params);

// Returns true if the AtMemory manual fallback item should be added to the
// context menu for `rfh` and `params`.
bool ShouldShowAtMemoryContextMenuItem(
    content::RenderFrameHost& rfh,
    const content::ContextMenuParams& params);

// Triggers the AtMemory search popup on the field identified by `params`.
void ExecuteAtMemoryContextMenuCommand(
    content::RenderFrameHost& rfh,
    const content::ContextMenuParams& params);

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_CONTEXT_MENU_UTILS_H_
