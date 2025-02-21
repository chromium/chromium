// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_install_ui_android.h"

#include "base/notimplemented.h"
#include "extensions/common/extension.h"

ExtensionInstallUIAndroid::ExtensionInstallUIAndroid(
    content::BrowserContext* context)
    : ExtensionInstallUI(context) {}

ExtensionInstallUIAndroid::~ExtensionInstallUIAndroid() = default;

void ExtensionInstallUIAndroid::OnInstallSuccess(
    scoped_refptr<const extensions::Extension> extension,
    const SkBitmap* icon) {
  // TODO(crbug.com/397754565): Implement this.
  NOTIMPLEMENTED() << "OnInstallSuccess";
}

void ExtensionInstallUIAndroid::OnInstallFailure(
    const extensions::CrxInstallError& error) {
  // TODO(crbug.com/397754565): Implement this.
  NOTIMPLEMENTED() << "OnInstallFailure";
}

// static
void ExtensionInstallUIAndroid::ShowBubble(
    scoped_refptr<const extensions::Extension> extension,
    const SkBitmap& icon) {
  // TODO(crbug.com/397754565): Implement this.
  NOTIMPLEMENTED() << "ShowBubble";
}
