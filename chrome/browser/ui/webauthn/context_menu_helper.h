// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBAUTHN_CONTEXT_MENU_HELPER_H_
#define CHROME_BROWSER_UI_WEBAUTHN_CONTEXT_MENU_HELPER_H_

#include <cstdint>

namespace content {
class RenderFrameHost;
}  // namespace content

namespace webauthn {

// Returns true iff the last focused field is an input field with
// autocomplete="webauthn" and a conditional WebAuthn request is pending.
bool IsPasskeyFromAnotherDeviceContextMenuEnabled(
    content::RenderFrameHost* render_frame_host,
    uint64_t form_renderer_id,
    uint64_t field_renderer_id);

// Triggered when the user selected the context menu item. This will show the
// WebAuthn dialog for passkeys from other devices and may include a QR code for
// hybrid flows or a prompt to plug in security keys.
void OnPasskeyFromAnotherDeviceContextMenuItemSelected(
    content::RenderFrameHost* render_frame_host);

}  // namespace webauthn

#endif  // CHROME_BROWSER_UI_WEBAUTHN_CONTEXT_MENU_HELPER_H_
