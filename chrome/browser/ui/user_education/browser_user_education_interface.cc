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

  // TODO(crbug.com/401757925): Remove after the issue is fixed.
#if BUILDFLAG(IS_MAC)
  if (!tab) {
    VLOG(0) << "Tab not found.";
  } else {
    auto* browser_window_interface = tab->GetBrowserWindowInterface();
    if (!browser_window_interface) {
      VLOG(0) << "BrowserWindowInferface not found.";
    } else {
      auto* user_education_interface =
          browser_window_interface->GetUserEducationInterface();
      if (!user_education_interface) {
        VLOG(0) << "UserEducationInterface not found.";
      }
      return user_education_interface;
    }
  }
  return nullptr;
#else
  return (tab && tab->GetBrowserWindowInterface())
             ? tab->GetBrowserWindowInterface()->GetUserEducationInterface()
             : nullptr;
#endif
}
