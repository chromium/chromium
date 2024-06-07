// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/permission_bubble/permission_prompt.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt.h"
#include "chrome/browser/ui/views/permissions/exclusive_access_permission_prompt.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_bubble.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_chip.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_quiet_icon.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/request_type.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "third_party/blink/public/common/features_generated.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/views/permissions/permission_prompt_notifications_mac.h"
#endif

namespace {

bool IsFullScreenMode(Browser* browser) {
  DCHECK(browser);

  // PWA uses the title bar as a substitute for LocationBarView.
  if (web_app::AppBrowserController::IsWebApp(browser))
    return false;

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  if (!browser_view)
    return false;

  LocationBarView* location_bar = browser_view->GetLocationBarView();

  return !location_bar || !location_bar->IsDrawn() ||
         location_bar->GetWidget()->GetTopLevelWidget()->IsFullscreen();
}

LocationBarView* GetLocationBarView(Browser* browser) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  return browser_view ? browser_view->GetLocationBarView() : nullptr;
}

// A permission request should be auto-ignored if:
//    - a user interacts with the LocationBar. The only exception is the NTP
//      page where the user needs to press on a microphone icon to get a
//      permission request.
//    - The Lens Overlay is open covering the context. We suppress permission
//      prompts until the Lens Overlay is closed.
bool ShouldIgnorePermissionRequest(
    content::WebContents* web_contents,
    Browser* browser,
    permissions::PermissionPrompt::Delegate* delegate) {
  DCHECK(web_contents);
  DCHECK(browser);

  // In case of the NTP, `WebContents::GetVisibleURL()` is equal to
  // `chrome://newtab/`, but the `LocationBarView` will be empty.
  if (web_contents->GetVisibleURL() == GURL(chrome::kChromeUINewTabURL)) {
    return false;
  }

  LocationBarView* location_bar = GetLocationBarView(browser);
  bool cant_display_prompt = location_bar && location_bar->IsEditingOrEmpty();

  LensOverlayController* lens_overlay_controller =
      browser->tab_strip_model()
          ->GetActiveTab()
          ->tab_features()
          ->lens_overlay_controller();
  // Don't show prompt if Lens Overlay is showing
  // TODO(b/331940245): Refactor to be decoupled from LensOverlayController
  if (lens_overlay_controller && lens_overlay_controller->IsOverlayShowing()) {
    cant_display_prompt = true;
  }

  permissions::PermissionUmaUtil::RecordPermissionPromptAttempt(
      delegate->Requests(), cant_display_prompt);

  return cant_display_prompt;
}

bool ShouldUseChip(permissions::PermissionPrompt::Delegate* delegate) {
  // Permission request chip should not be shown if `delegate->Requests()` were
  // requested without a user gesture.
  if (!permissions::PermissionUtil::HasUserGesture(delegate))
    return false;

  std::vector<raw_ptr<permissions::PermissionRequest, VectorExperimental>>
      requests = delegate->Requests();
  return base::ranges::all_of(
      requests, [](permissions::PermissionRequest* request) {
        return request
            ->GetRequestChipText(
                permissions::PermissionRequest::ChipTextType::LOUD_REQUEST)
            .has_value();
      });
}

bool IsLocationBarDisplayed(Browser* browser) {
  LocationBarView* lbv = GetLocationBarView(browser);
  return lbv && lbv->IsDrawn() &&
         !lbv->GetWidget()->GetTopLevelWidget()->IsFullscreen();
}

bool ShouldCurrentRequestUseQuietChip(
    permissions::PermissionPrompt::Delegate* delegate) {
  std::vector<raw_ptr<permissions::PermissionRequest, VectorExperimental>>
      requests = delegate->Requests();
  return base::ranges::all_of(
      requests, [](permissions::PermissionRequest* request) {
        return request->request_type() ==
                   permissions::RequestType::kNotifications ||
               request->request_type() ==
                   permissions::RequestType::kGeolocation;
      });
}

bool ShouldCurrentRequestUsePermissionElementSecondaryUI(
    permissions::PermissionPrompt::Delegate* delegate) {
  if (!base::FeatureList::IsEnabled(blink::features::kPermissionElement)) {
    return false;
  }

  std::vector<raw_ptr<permissions::PermissionRequest, VectorExperimental>>
      requests = delegate->Requests();
  return base::ranges::all_of(
      requests, [](permissions::PermissionRequest* request) {
        return (request->request_type() ==
                    permissions::RequestType::kCameraStream ||
                request->request_type() ==
                    permissions::RequestType::kGeolocation ||
                request->request_type() ==
                    permissions::RequestType::kMicStream) &&
               request->IsEmbeddedPermissionElementInitiated();
      });
}

bool ShouldCurrentRequestUseExclusiveAccessUI(
    permissions::PermissionPrompt::Delegate* delegate) {
  std::vector<raw_ptr<permissions::PermissionRequest, VectorExperimental>>
      requests = delegate->Requests();
  return base::ranges::all_of(
      requests, [](permissions::PermissionRequest* request) {
        return request->request_type() ==
                   permissions::RequestType::kPointerLock ||
               request->request_type() ==
                   permissions::RequestType::kKeyboardLock;
      });
}

std::unique_ptr<permissions::PermissionPrompt> CreatePwaPrompt(
    Browser* browser,
    content::WebContents* web_contents,
    permissions::PermissionPrompt::Delegate* delegate) {
  if (ShouldCurrentRequestUsePermissionElementSecondaryUI(delegate)) {
    return std::make_unique<EmbeddedPermissionPrompt>(browser, web_contents,
                                                      delegate);
  } else if (delegate->ShouldCurrentRequestUseQuietUI()) {
    return std::make_unique<PermissionPromptQuietIcon>(browser, web_contents,
                                                       delegate);
  } else {
    return std::make_unique<PermissionPromptBubble>(browser, web_contents,
                                                    delegate);
  }
}

std::unique_ptr<permissions::PermissionPrompt> CreateNormalPrompt(
    Browser* browser,
    content::WebContents* web_contents,
    permissions::PermissionPrompt::Delegate* delegate) {
  DCHECK(!delegate->ShouldCurrentRequestUseQuietUI());

  if (ShouldCurrentRequestUseExclusiveAccessUI(delegate)) {
    return std::make_unique<ExclusiveAccessPermissionPrompt>(
        browser, web_contents, delegate);
  } else if (ShouldCurrentRequestUsePermissionElementSecondaryUI(delegate)) {
    return std::make_unique<EmbeddedPermissionPrompt>(browser, web_contents,
                                                      delegate);
  } else if (ShouldUseChip(delegate) && IsLocationBarDisplayed(browser)) {
    return std::make_unique<PermissionPromptChip>(browser, web_contents,
                                                  delegate);
  } else {
    return std::make_unique<PermissionPromptBubble>(browser, web_contents,
                                                    delegate);
  }
}

std::unique_ptr<permissions::PermissionPrompt> CreateQuietPrompt(
    Browser* browser,
    content::WebContents* web_contents,
    permissions::PermissionPrompt::Delegate* delegate) {
  if (ShouldCurrentRequestUseQuietChip(delegate)) {
    if (IsLocationBarDisplayed(browser)) {
      return std::make_unique<PermissionPromptChip>(browser, web_contents,
                                                    delegate);
    } else {
      // If LocationBar is not displayed (Fullscreen mode), display a default
      // bubble only for non-abusive origins.
      DCHECK(!delegate->ShouldDropCurrentRequestIfCannotShowQuietly());
      return std::make_unique<PermissionPromptBubble>(browser, web_contents,
                                                      delegate);
    }
  } else {
    return std::make_unique<PermissionPromptQuietIcon>(browser, web_contents,
                                                       delegate);
  }
}

}  // namespace

std::unique_ptr<permissions::PermissionPrompt> CreatePermissionPrompt(
    content::WebContents* web_contents,
    permissions::PermissionPrompt::Delegate* delegate) {
  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  if (!browser) {
    DLOG(WARNING) << "Permission prompt suppressed because the WebContents is "
                     "not attached to any Browser window.";
    return nullptr;
  }

  if (delegate->ShouldDropCurrentRequestIfCannotShowQuietly() &&
      IsFullScreenMode(browser)) {
    return nullptr;
  }

  // Auto-ignore the permission request if a user is typing into location bar.
  if (ShouldIgnorePermissionRequest(web_contents, browser, delegate)) {
    return nullptr;
  }

#if BUILDFLAG(IS_MAC)
  // If this is a notification permission request coming from a PWA (or a PWA
  // associated tab), and this is the first time we show a permission prompt for
  // this request, try using a OS-native permission prompt. If showing the
  // prompt fails, it will trigger the view to be recreated for the request, at
  // which point we end up in the normal code path below.
  if (base::FeatureList::IsEnabled(features::kAppShimNotificationAttribution) &&
      !delegate->WasCurrentRequestAlreadyDisplayed() &&
      PermissionPromptNotificationsMac::CanHandleRequest(web_contents,
                                                         delegate)) {
    return std::make_unique<PermissionPromptNotificationsMac>(web_contents,
                                                              delegate);
  }
#endif

  if (web_app::AppBrowserController::IsWebApp(browser)) {
    return CreatePwaPrompt(browser, web_contents, delegate);
  } else if (delegate->ShouldCurrentRequestUseQuietUI()) {
    return CreateQuietPrompt(browser, web_contents, delegate);
  }
  return CreateNormalPrompt(browser, web_contents, delegate);
}
