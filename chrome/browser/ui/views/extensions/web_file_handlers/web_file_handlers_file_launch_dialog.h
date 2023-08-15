// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_WEB_FILE_HANDLERS_WEB_FILE_HANDLERS_FILE_LAUNCH_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_WEB_FILE_HANDLERS_WEB_FILE_HANDLERS_FILE_LAUNCH_DIALOG_H_

#include "base/functional/callback_forward.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/models/dialog_model.h"
#include "ui/views/widget/widget.h"

DECLARE_ELEMENT_IDENTIFIER_VALUE(kWebFileHandlersFileLaunchDialogCheckbox);

namespace extensions::file_handlers {

base::AutoReset<bool> SetDefaultRememberSelectionForTesting(
    bool remember_selection);

}  // namespace extensions::file_handlers

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_WEB_FILE_HANDLERS_WEB_FILE_HANDLERS_FILE_LAUNCH_DIALOG_H_
