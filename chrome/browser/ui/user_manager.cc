// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_manager.h"

UserManagerProfileDialog::BaseDialogDelegate::BaseDialogDelegate() = default;

bool UserManagerProfileDialog::BaseDialogDelegate::HandleContextMenu(
    content::RenderFrameHost* render_frame_host,
    const content::ContextMenuParams& params) {
  // Ignores context menu.
  return true;
}
