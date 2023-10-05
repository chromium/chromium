// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TAB_HELPERS_H_
#define CHROME_BROWSER_UI_TAB_HELPERS_H_

#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)

namespace android {
class TabWebContentsDelegateAndroid;
}

namespace thin_webview {
namespace android {
class ChromeThinWebViewInitializer;
}
}  // namespace thin_webview

#else

namespace chrome {
class BrowserTabStripModelDelegate;
}

#endif  // BUILDFLAG(IS_ANDROID)

namespace content {
class WebContents;
}

namespace prerender {
class ChromeNoStatePrefetchContentsDelegate;
}

// A "tab contents" is a WebContents that is used as a tab in a browser window
// (or the equivalent on Android). The TabHelpers class allows specific classes
// to attach the set of tab helpers that is used for tab contents.
//
// https://chromium.googlesource.com/chromium/src/+/main/docs/tab_helpers.md
//
// TODO(avi): This list is rather large, and for most callers it's due to the
// fact that they need tab helpers attached early to deal with arbitrary
// content loaded into a WebContents that will later be added to the tabstrip.
// Is there a better way to handle this? (Ideally, this list would contain
// only Browser and BrowserTabStripModelDelegate.)
class TabHelpers {
 private:
#if BUILDFLAG(IS_ANDROID)
  // ThinWebView is used to host WebContents on non-tab UIs in Android. Most
  // clients of ThinWebView will need a major subset of the tab helpers.
  friend class thin_webview::android::ChromeThinWebViewInitializer;

  friend class TabAndroid;
  friend class android::TabWebContentsDelegateAndroid;
#else
  friend class Browser;
  friend class chrome::BrowserTabStripModelDelegate;
#endif  // BUILDFLAG(IS_ANDROID)

  // chrome::Navigate creates WebContents that are destined for the tab strip,
  // and that might have WebUI that immediately calls back into random tab
  // helpers.
  friend class BrowserNavigatorWebContentsAdoption;

  // NoStatePrefetch and Prerendering load pages that have arbitrary external
  // content; they need the full set of tab helpers to deal with it.
  friend class prerender::ChromeNoStatePrefetchContentsDelegate;
  friend class PrerenderWebContentsDelegateImpl;

  // FYI: Do NOT add any more friends here. The functions above are the ONLY
  // ones that need to call AttachTabHelpers; if you think you do, re-read the
  // design document linked above, especially the section "Reusing tab helpers".

  // Adopts the specified WebContents as a full-fledged browser tab, attaching
  // all the associated tab helpers that are needed for the WebContents to
  // serve in that role. It is safe to call this on a WebContents that was
  // already adopted.
  static void AttachTabHelpers(content::WebContents* web_contents);
};

#endif  // CHROME_BROWSER_UI_TAB_HELPERS_H_
