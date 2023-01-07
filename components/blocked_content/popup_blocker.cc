// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/blocked_content/popup_blocker.h"

#include <string>

#include "base/check.h"
#include "base/command_line.h"
#include "components/blocked_content/popup_blocker_tab_helper.h"
#include "components/blocked_content/popup_navigation_delegate.h"
#include "components/blocked_content/safe_browsing_triggered_popup_blocker.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/embedder_support/switches.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-shared.h"

namespace blocked_content {
namespace {

content::Page& GetSourcePageForPopup(
    const content::OpenURLParams* open_url_params,
    content::WebContents* web_contents) {
  if (open_url_params) {
    content::RenderFrameHost* source = content::RenderFrameHost::FromID(

        open_url_params->source_render_process_id,
        open_url_params->source_render_frame_id);
    if (source)
      return source->GetPage();
  }

  // When there's no source RenderFrameHost, attribute the popup to the primary
  // page.
  return web_contents->GetPrimaryPage();
}

// If the popup should be blocked, returns the reason why it was blocked.
// Otherwise returns kNotBlocked.
PopupBlockType ShouldBlockPopup(content::WebContents* web_contents,
                                const GURL* opener_url,
                                bool user_gesture,
                                const content::OpenURLParams* open_url_params,
                                HostContentSettingsMap* settings_map) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          embedder_support::kDisablePopupBlocking)) {
    return PopupBlockType::kNotBlocked;
  }
  // If an explicit opener is not given, use the current committed load in this
  // web contents. This is because A page can't spawn popups (or do anything
  // else, either) until its load commits, so when we reach here, the popup was
  // spawned by the NavigationController's last committed entry, not the active
  // entry.  For example, if a page opens a popup in an onunload() handler, then
  // the active entry is the page to be loaded as we navigate away from the
  // unloading page.
  const GURL& url =
      opener_url ? *opener_url : web_contents->GetLastCommittedURL();
  ContentSetting cs;
  if (url.is_valid()) {
    cs = settings_map->GetContentSetting(url, url, ContentSettingsType::POPUPS);
  } else {
    cs = settings_map->GetDefaultContentSetting(ContentSettingsType::POPUPS,
                                                nullptr);
  }

  if (cs == CONTENT_SETTING_ALLOW)
    return PopupBlockType::kNotBlocked;

  if (!user_gesture)
    return PopupBlockType::kNoGesture;

  // This is trusted user action (e.g. shift-click), so make sure it is not
  // blocked.
  if (open_url_params &&
      open_url_params->triggering_event_info !=
          blink::mojom::TriggeringEventInfo::kFromUntrustedEvent) {
    return PopupBlockType::kNotBlocked;
  }

  auto* safe_browsing_blocker =
      SafeBrowsingTriggeredPopupBlocker::FromWebContents(web_contents);
  if (safe_browsing_blocker &&
      safe_browsing_blocker->ShouldApplyAbusivePopupBlocker(
          GetSourcePageForPopup(open_url_params, web_contents))) {
    return PopupBlockType::kAbusive;
  }
  return PopupBlockType::kNotBlocked;
}

}  // namespace

bool ConsiderForPopupBlocking(WindowOpenDisposition disposition) {
  return disposition == WindowOpenDisposition::NEW_POPUP ||
         disposition == WindowOpenDisposition::NEW_FOREGROUND_TAB ||
         disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB ||
         disposition == WindowOpenDisposition::NEW_WINDOW;
}

std::unique_ptr<PopupNavigationDelegate> MaybeBlockPopup(
    content::WebContents* web_contents,
    const GURL* opener_url,
    std::unique_ptr<PopupNavigationDelegate> delegate,
    const content::OpenURLParams* open_url_params,
    const blink::mojom::WindowFeatures& window_features,
    HostContentSettingsMap* settings_map) {
  DCHECK(web_contents);
  DCHECK(!open_url_params ||
         open_url_params->user_gesture == delegate->GetOriginalUserGesture());
  PopupBlockerTabHelper::LogAction(PopupBlockerTabHelper::Action::kInitiated);

  // Check |popup_blocker| first since it is cheaper than ShouldBlockPopup().
  auto* popup_blocker = PopupBlockerTabHelper::FromWebContents(web_contents);
  if (!popup_blocker)
    return delegate;

  PopupBlockType block_type = ShouldBlockPopup(
      web_contents, opener_url, delegate->GetOriginalUserGesture(),
      open_url_params, settings_map);
  if (block_type == PopupBlockType::kNotBlocked)
    return delegate;

  popup_blocker->AddBlockedPopup(std::move(delegate), window_features,
                                 block_type);
  return nullptr;
}

}  // namespace blocked_content
