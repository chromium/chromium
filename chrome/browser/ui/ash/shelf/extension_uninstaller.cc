// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/extension_uninstaller.h"

#include <string>

#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension.h"

ExtensionUninstaller::ExtensionUninstaller(Profile* profile,
                                           const std::string& extension_id,
                                           gfx::NativeWindow parent_window)
    : profile_(profile), app_id_(extension_id), parent_window_(parent_window) {}

ExtensionUninstaller::~ExtensionUninstaller() {}

void ExtensionUninstaller::Run() {
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile_)->GetInstalledExtension(
          app_id_);
  if (!extension) {
    CleanUp();
    return;
  }
  dialog_ = extensions::ExtensionUninstallDialog::Create(profile_,
                                                         parent_window_, this);
  dialog_->ConfirmUninstall(extension,
                            extensions::UNINSTALL_REASON_USER_INITIATED,
                            extensions::UNINSTALL_SOURCE_APP_LIST);
}

void ExtensionUninstaller::OnExtensionUninstallDialogClosed(
    bool did_start_uninstall,
    const std::u16string& error) {
  CleanUp();
}

void ExtensionUninstaller::CleanUp() {
  delete this;
}
