// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/permission_prompt_factory.h"

#include <algorithm>
#include <memory>

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/location_bar/location_bar_override_data.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/permission_bubble/permission_prompt.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_content_scrim_view.h"
#include "chrome/browser/ui/views/permissions/exclusive_access_permission_prompt.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_bubble.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_chip.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_quiet_icon.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/webui_url_constants.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/permissions_client.h"
#include "components/permissions/request_type.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "third_party/blink/public/common/features_generated.h"
#include "url/origin.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/views/permissions/permission_prompt_notifications_mac.h"
#include "chrome/browser/web_applications/os_integration/mac/web_app_shortcut_mac.h"
#endif

namespace {

BrowserWindowInterface* GetBrowser(content::WebContents* web_contents) {
  tabs::TabInterface* tab =
      web_contents ? tabs::TabInterface::MaybeGetFromContents(web_contents)
                   : nullptr;
  BrowserWindowInterface* browser_window_interface =
      tab ? tab->GetBrowserWindowInterface() : nullptr;
  if (!browser_window_interface) {
    browser_window_interface = webui::GetBrowserWindowInterface(web_contents);
  }
  return browser_window_interface;
}

bool IsFullScreenMode(content::WebContents* web_contents) {
  BrowserWindowInterface* browser = GetBrowser(web_contents);
  if (!browser) {
    return false;
  }

  // PWA uses the title bar as a substitute for LocationBarView.
  if (web_app::AppBrowserController::IsWebApp(browser)) {
    return false;
  }

  LocationBar* location_bar =
      ::location_bar::GetLocationBarForWebContents(web_contents);

  return !location_bar || !location_bar->IsDrawn() ||
         location_bar->IsFullscreen();
}


// A permission request should be auto-ignored if:
//    - a user interacts with the LocationBar. The only exception is the NTP
//      page where the user needs to press on a microphone icon to get a
//      permission request.
//    - The Lens Overlay is open covering the context. We suppress permission
//      prompts until the Lens Overlay is closed.
bool ShouldIgnorePermissionRequest(
    content::WebContents* web_contents,
    permissions::PermissionPrompt::Delegate* delegate) {
  DCHECK(web_contents);

  if (ShouldShowPermissionPromptEvenIfOmniboxEditedOrEmpty(web_contents)) {
    return false;
  }

  // Suppress permission prompts if the omnibox is being edited.
  LocationBar* location_bar =
      ::location_bar::GetLocationBarForWebContents(web_contents);
  bool can_display_prompt =
      !(location_bar && location_bar->GetOmniboxController() &&
        location_bar->GetOmniboxController()
            ->edit_model()
            ->user_input_in_progress());

  BrowserWindowInterface* browser = GetBrowser(web_contents);
  if (browser) {
    LensOverlayController* lens_overlay_controller =
        browser->GetTabStripModel()
            ->GetActiveTab()
            ->GetTabFeatures()
            ->lens_overlay_controller();
    // Don't show prompt if Lens Overlay is showing
    // TODO(b/331940245): Refactor to be decoupled from LensOverlayController
    if (lens_overlay_controller &&
        lens_overlay_controller->IsOverlayShowing()) {
      can_display_prompt = false;
    }
  }

  permissions::PermissionUmaUtil::RecordPermissionPromptAttempt(
      delegate->Requests(), can_display_prompt);

  return !can_display_prompt;
}

bool ShouldUseChip(permissions::PermissionPrompt::Delegate* delegate) {
  // Permission request chip should not be shown if `delegate->Requests()` were
  // requested without a user gesture.
  if (!permissions::PermissionUtil::HasUserGesture(delegate)) {
    return false;
  }

  const auto& requests = delegate->Requests();
  return std::ranges::all_of(
      requests, [](const auto& request) {
        return request
            ->GetRequestChipText(
                permissions::PermissionRequest::ChipTextType::LOUD_REQUEST)
            .has_value();
      });
}

bool IsLocationBarDisplayed(content::WebContents* web_contents) {
  LocationBar* lb = ::location_bar::GetLocationBarForWebContents(web_contents);
  return lb && lb->IsDrawn() && !lb->IsFullscreen();
}

bool ShouldCurrentRequestUseQuietChip(
    permissions::PermissionPrompt::Delegate* delegate) {
  const auto& requests = delegate->Requests();
  return std::ranges::all_of(
      requests, [](const auto& request) {
        return request->request_type() ==
                   permissions::RequestType::kNotifications ||
               request->request_type() ==
                   permissions::RequestType::kGeolocation;
      });
}

bool ShouldCurrentRequestUseExclusiveAccessUI(
    permissions::PermissionPrompt::Delegate* delegate) {
  const auto& requests = delegate->Requests();
  return permissions::feature_params::kKeyboardLockPromptUIStyle.Get() &&
         std::ranges::all_of(
             requests, [](const auto& request) {
               return request->request_type() ==
                          permissions::RequestType::kPointerLock ||
                      request->request_type() ==
                          permissions::RequestType::kKeyboardLock;
             });
}

bool CanCurrentRequestUseModalUI(
    content::WebContents* web_contents,
    permissions::PermissionPrompt::Delegate* delegate) {
  // Permission prompts from side panels and omnibox popup do not
  // have a TabInterface since they are not tab-based, but there is never a
  // concern with showing a modal on them because if they can request
  // permission, they are open and available unlike tabs, which can be switched
  // between.
  // This function is only called after embedded permission prompt has
  // been chosen as the prompt type. Embedded permission prompt path is not run
  // for WebUIs like omnibox popup/side panels if the omnibox embedded
  // permission flag is not enabled, so, in those cases, accessing a null
  // `TabInterface` is avoided.
  if (permissions::PermissionsClient::Get()
          ->IsPrivilegedInternalWebUIOrNewTabPage(
              web_contents, delegate->GetRequestingOrigin(),
              /*already_overrode_requester=*/true)) {
    return true;
  }
  if (delegate->GetEmbeddedPromptFlowModel() &&
      delegate->GetEmbeddedPromptFlowModel()->prompt_content_scrim()) {
    // We are already displaying the scrim for an embedded permission prompt.
    return true;
  }
  return tabs::TabInterface::GetFromContents(web_contents)->CanShowModalUI();
}

std::unique_ptr<permissions::PermissionPrompt> CreatePwaPrompt(
    content::WebContents* web_contents,
    permissions::PermissionPrompt::Delegate* delegate) {
  if (permissions::PermissionUtil::
          ShouldCurrentRequestUsePermissionElementSecondaryUI(delegate)) {
    // Run this check inside the if statement to avoid creating another prompt
    // type for embedded permission prompts.
    return CanCurrentRequestUseModalUI(web_contents, delegate)
               ? std::make_unique<EmbeddedPermissionPrompt>(web_contents,
                                                            delegate)
               : nullptr;
  } else if (delegate->ShouldCurrentRequestUseQuietUI()) {
    return std::make_unique<PermissionPromptQuietIcon>(web_contents, delegate);
  } else {
    return std::make_unique<PermissionPromptBubble>(web_contents, delegate);
  }
}

std::unique_ptr<permissions::PermissionPrompt> CreateNormalPrompt(
    content::WebContents* web_contents,
    permissions::PermissionPrompt::Delegate* delegate) {
  DCHECK(!delegate->ShouldCurrentRequestUseQuietUI());
  if (ShouldCurrentRequestUseExclusiveAccessUI(delegate)) {
    return CanCurrentRequestUseModalUI(web_contents, delegate)
               ? std::make_unique<ExclusiveAccessPermissionPrompt>(web_contents,
                                                                   delegate)
               : nullptr;
  } else if (permissions::PermissionUtil::
                 ShouldCurrentRequestUsePermissionElementSecondaryUI(
                     delegate, web_contents)) {
    // Run this check inside the if statement to avoid creating another prompt
    // type for embedded permission prompts.
    return CanCurrentRequestUseModalUI(web_contents, delegate)
               ? std::make_unique<EmbeddedPermissionPrompt>(web_contents,
                                                            delegate)
               : nullptr;
  } else if (ShouldUseChip(delegate) && IsLocationBarDisplayed(web_contents)) {
    return std::make_unique<PermissionPromptChip>(web_contents, delegate);
  } else {
    return std::make_unique<PermissionPromptBubble>(web_contents, delegate);
  }
}

std::unique_ptr<permissions::PermissionPrompt> CreateQuietPrompt(
    content::WebContents* web_contents,
    permissions::PermissionPrompt::Delegate* delegate) {
  if (ShouldCurrentRequestUseQuietChip(delegate)) {
    if (IsLocationBarDisplayed(web_contents)) {
      return std::make_unique<PermissionPromptChip>(web_contents, delegate);
    } else {
      // If LocationBar is not displayed (Fullscreen mode), display a default
      // bubble only for non-abusive origins.
      DCHECK(!delegate->ShouldDropCurrentRequestIfCannotShowQuietly());
      return std::make_unique<PermissionPromptBubble>(web_contents, delegate);
    }
  } else {
    return std::make_unique<PermissionPromptQuietIcon>(web_contents, delegate);
  }
}

}  // namespace

bool ShouldShowPermissionPromptEvenIfOmniboxEditedOrEmpty(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return false;
  }

  // Allow permission prompts for WebUI pages that should bypass the omnibox
  // empty or editing state check:
  // - NTP has an empty omnibox.
  // - Contextual Tasks Tab has an empty omnibox.
  // - Omnibox Popup is an embedded WebUI that itself may request permissions.
  // - Omnibox Everywhere is an embedded WebUI that itself may request
  // permissions.
  const url::Origin committed_origin =
      web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin();
  if (committed_origin.IsSameOriginWith(chrome::ChromeUINewTabURLAsGURL()) ||
      committed_origin.IsSameOriginWith(
          chrome::ChromeUINewTabPageURLAsGURL()) ||
      committed_origin.IsSameOriginWith(
          GURL(chrome::kChromeUIOmniboxPopupURL)) ||
      committed_origin.IsSameOriginWith(
          GURL(chrome::kChromeUIContextualTasksURL)) ||
      committed_origin.IsSameOriginWith(
          GURL(chrome::kChromeUIOmniboxEverywhereURL))) {
    return true;
  }
  return false;
}

std::unique_ptr<permissions::PermissionPrompt> CreatePermissionPrompt(
    content::WebContents* web_contents,
    permissions::PermissionPrompt::Delegate* delegate) {
  if (delegate->ShouldDropCurrentRequestIfCannotShowQuietly() &&
      IsFullScreenMode(web_contents)) {
    return nullptr;
  }

  if (ShouldIgnorePermissionRequest(web_contents, delegate)) {
    return nullptr;
  }

#if BUILDFLAG(IS_MAC)
  // If this is a notification permission request coming from a PWA (or a PWA
  // associated tab), and this is the first time we show a permission prompt for
  // this request, try using a OS-native permission prompt. If showing the
  // prompt fails, it will trigger the view to be recreated for the request, at
  // which point we end up in the normal code path below.
  if (web_app::UseNotificationAttributionForWebAppShims() &&
      !delegate->WasCurrentRequestAlreadyDisplayed() &&
      PermissionPromptNotificationsMac::CanHandleRequest(web_contents,
                                                         delegate)) {
    return std::make_unique<PermissionPromptNotificationsMac>(web_contents,
                                                              delegate);
  }
#endif

  if (web_app::AppBrowserController::IsWebApp(GetBrowser(web_contents))) {
    return CreatePwaPrompt(web_contents, delegate);
  } else if (delegate->ShouldCurrentRequestUseQuietUI()) {
    return CreateQuietPrompt(web_contents, delegate);
  }

  // If omnibox is open and this is a microphone request, notify the
  // `LocationBar` (and Omnibox presenter) that a permission prompt is starting
  // right before constructing the prompt view widget. This ensures the omnibox
  // ignores focus-loss events during the time that the permission prompt is
  // showing.
  bool has_mic_request =
      std::ranges::any_of(delegate->Requests(), [](const auto& request) {
        return request->request_type() == permissions::RequestType::kMicStream;
      });

  if (has_mic_request) {
    if (LocationBar* location_bar =
            ::location_bar::GetLocationBarForWebContents(web_contents)) {
      location_bar->SetPermissionPromptShowing(true);
    }
  }

  auto prompt = CreateNormalPrompt(web_contents, delegate);

  if (!prompt && has_mic_request) {
    if (LocationBar* location_bar =
            ::location_bar::GetLocationBarForWebContents(web_contents)) {
      location_bar->SetPermissionPromptShowing(false);
    }
  }

  return prompt;
}

std::unique_ptr<
    permissions::EmbeddedPermissionPromptFlowModel::PromptContentScrim>
CreatePermissionPromptContentScrim(
    content::WebContents& web_contents,
    permissions::EmbeddedPermissionPromptFlowModel* flow_model) {
  return std::make_unique<EmbeddedPermissionPromptContentScrim>(web_contents,
                                                                flow_model);
}
