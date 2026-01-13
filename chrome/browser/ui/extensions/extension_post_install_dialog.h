// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_POST_INSTALL_DIALOG_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_POST_INSTALL_DIALOG_H_

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/ui/extensions/extension_install_ui.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

class Profile;
class SkBitmap;

namespace content {
class WebContents;
}

namespace extensions {

// Triggers the post-install dialog for an extension.
void TriggerPostInstallDialog(
    Profile* profile,
    scoped_refptr<const extensions::Extension> extension,
    const SkBitmap& icon,
    base::OnceCallback<content::WebContents*()> get_web_contents_callback);

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_POST_INSTALL_DIALOG_H_
