// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_post_install_dialog_utils.h"

#include <string>

#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/extensions/extension_installed_watcher.h"
#include "chrome/browser/ui/extensions/extension_post_install_dialog.h"
#include "chrome/browser/ui/extensions/extension_post_install_dialog_model.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "extensions/common/extension.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace extensions {

void TriggerPostInstallDialog(
    Profile* profile,
    scoped_refptr<const extensions::Extension> extension,
    const SkBitmap& icon,
    base::OnceCallback<content::WebContents*()> get_web_contents_callback) {
  auto watcher = std::make_unique<ExtensionInstalledWatcher>(profile);
  ExtensionInstalledWatcher* watcher_ptr = watcher.get();
  watcher_ptr->WaitForInstall(
      extension->id(),
      base::BindOnce(
          [](std::unique_ptr<ExtensionInstalledWatcher> watcher,
             scoped_refptr<const extensions::Extension> ext, Profile* prof,
             const SkBitmap& icon_val,
             base::OnceCallback<content::WebContents*()> get_web_contents_cb,
             bool installed) {
            if (!installed) {
              return;
            }
            content::WebContents* web_contents =
                std::move(get_web_contents_cb).Run();
            if (!web_contents) {
              return;
            }
            auto model = std::make_unique<ExtensionPostInstallDialogModel>(
                prof, ext.get(), icon_val);
            extensions::ShowExtensionPostInstallDialog(prof, web_contents,
                                                       std::move(model));
          },
          std::move(watcher), extension, profile, icon,
          std::move(get_web_contents_callback)));
}

}  // namespace extensions
