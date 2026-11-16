// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/shortcuts/desktop_shortcuts_utils.h"

#include "chrome/browser/shortcuts/shortcut_creator.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"

namespace shortcuts {

bool CanCreateDesktopShortcut(content::WebContents* web_contents) {
  if (!web_contents) {
    return false;
  }

  auto* const tab = tabs::TabInterface::MaybeGetFromContents(web_contents);
  if (!tab || !tab->GetBrowserWindowInterface()) {
    return false;
  }

  return CanCreateShortcutForWebContents(web_contents);
}

}  // namespace shortcuts
