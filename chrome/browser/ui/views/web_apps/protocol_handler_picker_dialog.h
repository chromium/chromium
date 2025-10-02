// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_PROTOCOL_HANDLER_PICKER_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_PROTOCOL_HANDLER_PICKER_DIALOG_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/image_model.h"
#include "url/gurl.h"
#include "url/origin.h"

DECLARE_ELEMENT_IDENTIFIER_VALUE(
    kProtocolHandlerPickerDialogRememberSelectionCheckboxId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kProtocolHandlerPickerDialogOkButtonId);

namespace web_app {

struct ProtocolHandlerPickerDialogEntry {
  std::string app_id;
  std::u16string app_name;
  ui::ImageModel icon;
};

using ProtocolHandlerPickerDialogEntries =
    std::vector<ProtocolHandlerPickerDialogEntry>;

struct ProtocolHandlerPickerDialogResult {
  std::string selected_app_id;
  bool remember_choice;
};

// The callback to be run when the dialog is closed for any reason.
// The optional will be empty (std::nullopt) if the user canceled or the dialog
// was destroyed for some reason.
using OnPickerClosedCallback =
    base::OnceCallback<void(std::optional<ProtocolHandlerPickerDialogResult>)>;

std::unique_ptr<ui::DialogModel> CreateProtocolHandlerPickerDialog(
    const GURL& protocol_url,
    const ProtocolHandlerPickerDialogEntries& apps,
    const std::optional<url::Origin>& initiator_origin,
    OnPickerClosedCallback callback);

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_PROTOCOL_HANDLER_PICKER_DIALOG_H_
