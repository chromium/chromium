// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_education/browser_user_education_interface.h"

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"

// static
BrowserUserEducationInterface*
BrowserUserEducationInterface::MaybeGetForWebContentsInTab(
    content::WebContents* contents) {
  auto* const tab = tabs::TabInterface::MaybeGetFromContents(contents);
  return (tab && tab->GetBrowserWindowInterface())
             ? tab->GetBrowserWindowInterface()->GetUserEducationInterface()
             : nullptr;
}
