// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chrome_web_contents_handler.h"

#include <utility>

#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "components/captive_portal/core/buildflags.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom.h"
#include "ui/base/base_window.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
#include "base/check.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/captive_portal/captive_portal_service_factory.h"
#include "components/captive_portal/content/captive_portal_service.h"
#include "components/captive_portal/content/captive_portal_tab_helper.h"
#endif

using content::BrowserContext;
using content::OpenURLParams;
using content::WebContents;

ChromeWebContentsHandler::ChromeWebContentsHandler(
    captive_portal::CaptivePortalWindowType captive_portal_window_type)
    : captive_portal_window_type_(captive_portal_window_type) {}

ChromeWebContentsHandler::~ChromeWebContentsHandler() = default;

// Opens a new URL inside |source|. |context| is the browser context that the
// browser should be owned by. |params| contains the URL to open and various
// attributes such as disposition. Returns the WebContents opened by the browser
// on success. Otherwise, returns nullptr.
WebContents* ChromeWebContentsHandler::OpenURLFromTab(
    content::BrowserContext* context,
    WebContents* source,
    const OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  if (!context) {
    return nullptr;
  }

  Profile* profile = Profile::FromBrowserContext(context);

  BrowserWindowInterface* browser =
      ProfileBrowserCollection::GetForProfile(profile)->FindTabbedBrowser();
  const bool browser_created = !browser;
  if (!browser) {
    if (GetBrowserWindowCreationStatusForProfile(*profile) !=
        BrowserWindowInterface::CreationStatus::kOk) {
      return nullptr;
    }
    // TODO(erg): OpenURLParams should pass a user_gesture flag, pass it to
    // CreateParams, and pass the real value to nav_params below.
    browser = CreateBrowserWindow(BrowserWindowCreateParams(
        BrowserWindowInterface::TYPE_NORMAL, profile, true));
  }
  NavigateParams nav_params(browser, params.url, params.transition);
  nav_params.FillNavigateParamsFromOpenURLParams(params);
  if (source && source->IsCrashed() &&
      params.disposition == WindowOpenDisposition::kCurrentTab &&
      ui::PageTransitionCoreTypeIs(params.transition,
                                   ui::PAGE_TRANSITION_LINK)) {
    nav_params.disposition = WindowOpenDisposition::kNewForegroundTab;
  } else {
    nav_params.disposition = params.disposition;
  }
  nav_params.window_action = NavigateParams::WindowAction::kShowWindow;
  base::WeakPtr<content::NavigationHandle> navigation_handle =
      Navigate(&nav_params);
  if (navigation_handle_callback && navigation_handle) {
    std::move(navigation_handle_callback).Run(*navigation_handle);
  }

  // Close the browser if chrome::Navigate created a new one.
  if (browser_created && (browser != nav_params.browser)) {
    browser->GetWindow()->Close();
  }

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
  if (!context) {
    return;
  }

  Profile* profile = Profile::FromBrowserContext(context);

  BrowserWindowInterface* browser =
      ProfileBrowserCollection::GetForProfile(profile)->FindTabbedBrowser();
  const bool browser_created = !browser;
  if (!browser) {
    // The request can be triggered by Captive portal when browser is not ready
    // (https://crbug.com/40154317).
    if (GetBrowserWindowCreationStatusForProfile(*profile) !=
        BrowserWindowInterface::CreationStatus::kOk) {
      return;
    }
    browser = CreateBrowserWindow(BrowserWindowCreateParams(
        BrowserWindowInterface::TYPE_NORMAL, profile, user_gesture));
  }
  NavigateParams params(browser, std::move(new_contents));
  params.source_contents = source;
  params.disposition = disposition;
  params.window_features = window_features;
  params.window_action = NavigateParams::WindowAction::kShowWindow;
  params.user_gesture = user_gesture;
  Navigate(&params);

  // Close the browser if chrome::Navigate created a new one.
  if (browser_created && (browser != params.browser)) {
    browser->GetWindow()->Close();
  }
}

void ChromeWebContentsHandler::RunFileChooser(
    content::RenderFrameHost* render_frame_host,
    scoped_refptr<content::FileSelectListener> listener,
    const blink::mojom::FileChooserParams& params) {
  Profile* profile =
      Profile::FromBrowserContext(render_frame_host->GetBrowserContext());
  if (!profile || GetBrowserWindowCreationStatusForProfile(*profile) !=
                      BrowserWindowInterface::CreationStatus::kOk) {
    listener->FileSelectionCanceled();
    return;
  }
  FileSelectHelper::RunFileChooser(render_frame_host, std::move(listener),
                                   params);
}

void ChromeWebContentsHandler::OnDialogWebContentsCreated(
    content::WebContents* web_contents) {
#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  if (captive_portal_window_type_ ==
      captive_portal::CaptivePortalWindowType::kNone) {
    return;
  }
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  captive_portal::CaptivePortalService* captive_portal_service =
      CaptivePortalServiceFactory::GetForProfile(profile);
  // CaptivePortalServiceFactory builds a service for every profile a dialog
  // can be shown for, including the ash-internal sign-in and lock screen
  // profiles.
  CHECK(captive_portal_service);
  // This dialog is itself the login surface, so there is no login tab to open.
  // The callback must still be non-null; CaptivePortalTabReloader runs it.
  captive_portal::CaptivePortalTabHelper::CreateForWebContents(
      web_contents, captive_portal_service, base::DoNothing());
  // See NetworkPortalSigninWindow::Show() for what the window type affects.
  captive_portal::CaptivePortalTabHelper::FromWebContents(web_contents)
      ->set_window_type(captive_portal_window_type_);
#endif  // BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
}
