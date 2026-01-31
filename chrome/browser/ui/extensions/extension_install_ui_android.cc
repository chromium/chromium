// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_install_ui_android.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/notimplemented.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/extensions/extension_dialog_utils.h"
#include "chrome/browser/ui/extensions/extension_installed_watcher.h"
#include "chrome/browser/ui/extensions/extension_post_install_dialog.h"
#include "chrome/browser/ui/extensions/extension_post_install_dialog_model.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/models/dialog_model.h"

namespace {
content::WebContents* GetWebContentsForProfile(Profile* profile) {
  for (const TabModel* tab_model : TabModelList::models()) {
    if (tab_model->GetProfile() != profile) {
      continue;
    }

    int tab_count = tab_model->GetTabCount();
    for (int i = 0; i < tab_count; i++) {
      content::WebContents* web_contents = tab_model->GetWebContentsAt(i);
      if (web_contents) {
        return web_contents;
      }
    }
  }
  return nullptr;
}

}  // namespace

ExtensionInstallUIAndroid::ExtensionInstallUIAndroid(
    content::BrowserContext* context)
    : ExtensionInstallUI(context) {}

ExtensionInstallUIAndroid::~ExtensionInstallUIAndroid() = default;

void ExtensionInstallUIAndroid::OnInstallSuccess(
    scoped_refptr<const extensions::Extension> extension,
    const SkBitmap* icon) {
  if (IsUiDisabled() || skip_post_install_ui() || extension->is_theme()) {
    return;
  }

  if (!profile()) {
    return;
  }

  Profile* current_profile = profile()->GetOriginalProfile();

  SkBitmap icon_to_use = icon ? *icon : SkBitmap();
  extensions::TriggerPostInstallDialog(
      current_profile, extension, icon_to_use,
      base::BindOnce(&GetWebContentsForProfile, current_profile));
}

void ExtensionInstallUIAndroid::OnInstallFailure(
    const extensions::CrxInstallError& error) {
  // TODO(crbug.com/397754565): Implement this.
  NOTIMPLEMENTED() << "OnInstallFailure";
}
