// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/universal_web_contents_observers.h"

#include "chrome/browser/extensions/chrome_content_browser_client_extensions_part.h"

#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#endif

void AttachUniversalWebContentsObservers(content::WebContents* web_contents) {
  // This function is for attaching *universal* WebContentsObservers - ones that
  // should be attached to *every* WebContents.  Such universal observers and/or
  // helpers are relatively rare and therefore only a limited set of observers
  // should be handled below.
  //
  // In particular, helpers handled by TabHelpers::AttachTabHelpers typically
  // only apply to tabs, but not to other flavors of WebContents.  As pointed
  // out by //docs/tab_helpers.md there are WebContents that are not tabs
  // and not every WebContents has (or needs) every tab helper.

#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (extensions::ChromeContentBrowserClientExtensionsPart::
          AreExtensionsDisabledForProfile(web_contents->GetBrowserContext())) {
    return;
  }

  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      web_contents);
#endif
}
