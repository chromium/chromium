// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_dialog_utils.h"

#include <utility>

#include "content/public/browser/web_contents.h"
#include "ui/android/modal_dialog_wrapper.h"
#include "ui/base/models/dialog_model.h"

void ShowModalDialog(gfx::NativeWindow parent,
                     std::unique_ptr<ui::DialogModel> dialog_model) {
  ui::ModalDialogWrapper::ShowTabModal(std::move(dialog_model), parent);
}

void ShowDialog(gfx::NativeWindow parent,
                const extensions::ExtensionId& /* extension_id */,
                std::unique_ptr<ui::DialogModel> dialog_model) {
  // We ignore `extension_id` as dialogs are never anchored to the extension
  // action button on Android for UX reasons.
  ShowModalDialog(parent, std::move(dialog_model));
}

void ShowDialog(gfx::NativeWindow parent,
                const std::vector<extensions::ExtensionId>& /* extension_ids */,
                std::unique_ptr<ui::DialogModel> dialog_model) {
  // We ignore `extension_ids` as dialogs are never anchored to the extension
  // action button on Android for UX reasons.
  ShowModalDialog(parent, std::move(dialog_model));
}

void ShowWebModalDialog(content::WebContents* web_contents,
                        std::unique_ptr<ui::DialogModel> dialog_model) {
  ShowModalDialog(web_contents->GetTopLevelNativeWindow(),
                  std::move(dialog_model));
}
