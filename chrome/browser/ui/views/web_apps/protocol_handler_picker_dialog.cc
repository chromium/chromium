// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/protocol_handler_picker_dialog.h"

#include <optional>

namespace web_app {

std::unique_ptr<ui::DialogModel> CreateProtocolHandlerPickerDialog(
    const GURL& protocol_url,
    const ProtocolHandlerPickerDialogEntries& apps,
    const std::optional<url::Origin>& initiator_origin,
    OnPickerClosedCallback callback) {
  // TODO(crbug.com/422422887): Implement the dialog.
  std::move(callback).Run({{.selected_app_id = apps[0].app_id}});
  return nullptr;
}

}  // namespace web_app
