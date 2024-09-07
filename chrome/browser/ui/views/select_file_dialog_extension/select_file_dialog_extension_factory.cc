// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/select_file_dialog_extension/select_file_dialog_extension_factory.h"

#include "chrome/browser/ui/views/select_file_dialog_extension/select_file_dialog_extension.h"
#include "ui/shell_dialogs/select_file_policy.h"

SelectFileDialogExtensionFactory::SelectFileDialogExtensionFactory() {
}

SelectFileDialogExtensionFactory::~SelectFileDialogExtensionFactory() {
}

ui::SelectFileDialog* SelectFileDialogExtensionFactory::Create(
    ui::SelectFileDialog::Listener* listener,
    std::unique_ptr<ui::SelectFilePolicy> policy) {
  return SelectFileDialogExtension::Create(listener, std::move(policy));
}
