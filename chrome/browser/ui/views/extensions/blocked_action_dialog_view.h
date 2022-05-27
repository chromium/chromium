// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_BLOCKED_ACTION_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_BLOCKED_ACTION_DIALOG_VIEW_H_

#include "base/callback_forward.h"
#include "extensions/common/extension_id.h"

class Browser;

static void ShowBlockedActionDialogView(
    Browser* browser,
    const extensions::ExtensionId& extension_id,
    bool show_checkbox,
    base::OnceCallback<void(bool)> callback);

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_BLOCKED_ACTION_DIALOG_VIEW_H_
