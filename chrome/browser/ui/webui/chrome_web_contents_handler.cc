// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chrome_web_contents_handler.h"

#include <utility>

#include "build/chromeos_buildflags.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom.h"

using content::BrowserContext;
using content::OpenURLParams;
using content::WebContents;

ChromeWebContentsHandler::ChromeWebContentsHandler() {
}

ChromeWebContentsHandler::~ChromeWebContentsHandler() {
}

// Opens a new URL inside |source|. |context| is the browser context that the
// browser should be owned by. |params| contains the URL to open and various
// attributes such as disposition. Returns the WebContents opened by the browser
// on success. Otherwise, returns nullptr. In ChromeOS Ash, the URL might be
// opened in Lacros. In that case, this function returns nullptr.
WebContents* ChromeWebContentsHandler::OpenURLFromTab(
    content::BrowserContext* context,
    WebContents* source,
    const OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  if (!context)
    return nullptr;

  Profile* profile = Profile::FromBrowserContext(context);

  Browser* browser = chrome::FindTabbedBrowser(profile, false);
  const bool browser_created = !browser;
  if (!browser) {
    if (Browser::GetCreationStatusForProfile(profile) !=
        Browser::CreationStatus::kOk) {
      return nullptr;
    }
    // TODO(erg): OpenURLParams should pass a user_gesture flag, pass it to
    // CreateParams, and pass the real value to nav_params below.
    browser = Browser::Create(
        Browser::CreateParams(Browser::TYPE_NORMAL, profile, true));
  }
  NavigateParams nav_params(browser, params.url, params.transition);
  nav_params.FillNavigateParamsFromOpenURLParams(params);
  if (source && source->IsCrashed() &&
      params.disposition == WindowOpenDisposition::CURRENT_TAB &&
      ui::PageTransitionCoreTypeIs(params.transition,
                                   ui::PAGE_TRANSITION_LINK)) {
    nav_params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  } else {
    nav_params.disposition = params.disposition;
  }
  nav_params.window_action = NavigateParams::SHOW_WINDOW;
  base::WeakPtr<content::NavigationHandle> navigation_handle =
      Navigate(&nav_params);
  if (navigation_handle_callback && navigation_handle) {
    std::move(navigation_handle_callback).Run(*navigation_handle);
  }

  // Close the browser if chrome::Navigate created a new one.
  if (browser_created && (browser != nav_params.browser))
    browser->window()->Close();

  return nav_params.navigated_or_inserted_contents;
}

// Creates a new tab with |new_contents|. |context| is the browser context that
// the browser should be owned by. |source| is the WebContent where the
// operation originated. |disposition| controls how the new tab should be
// opened. |initial_rect| is the position and size of the window if a new window
// is created.  |user_gesture| is true if the operation was started by a user
// gesture.
void ChromeWebContentsHandler::AddNewContents(
    content::BrowserContext* context,
    WebContents* source,
    std::unique_ptr<WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    bool user_gesture) {
  if (!context)
    return;

  Profile* profile = Profile::FromBrowserContext(context);

  Browser* browser = chrome::FindTabbedBrowser(profile, false);
  const bool browser_created = !browser;
  if (!browser) {
    // The request can be triggered by Captive portal when browser is not ready
    // (https://crbug.com/1141608).
    if (Browser::GetCreationStatusForProfile(profile) !=
        Browser::CreationStatus::kOk) {
      return;
    }
    browser = Browser::Create(
        Browser::CreateParams(Browser::TYPE_NORMAL, profile, user_gesture));
  }
  NavigateParams params(browser, std::move(new_contents));
  params.source_contents = source;
  params.disposition = disposition;
  params.window_features = window_features;
  params.window_action = NavigateParams::SHOW_WINDOW;
  params.user_gesture = user_gesture;
  Navigate(&params);

  // Close the browser if chrome::Navigate created a new one.
  if (browser_created && (browser != params.browser))
    browser->window()->Close();
}

void ChromeWebContentsHandler::RunFileChooser(
    content::RenderFrameHost* render_frame_host,
    scoped_refptr<content::FileSelectListener> listener,
    const blink::mojom::FileChooserParams& params) {
  FileSelectHelper::RunFileChooser(render_frame_host, std::move(listener),
                                   params);
}
