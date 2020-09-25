// Copyright 2018 The Chromium Authors. All rights reserved.
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
#include "components/safe_browsing/content/triggers/ad_popup_trigger.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace blocked_content {
namespace {

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
    cs = settings_map->GetContentSetting(url, url, ContentSettingsType::POPUPS,
                                         std::string());
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
  if (open_url_params && open_url_params->triggering_event_info !=
                             blink::TriggeringEventInfo::kFromUntrustedEvent) {
    return PopupBlockType::kNotBlocked;
  }

  auto* safe_browsing_blocker =
      SafeBrowsingTriggeredPopupBlocker::FromWebContents(web_contents);
  if (safe_browsing_blocker &&
      safe_browsing_blocker->ShouldApplyAbusivePopupBlocker()) {
    return PopupBlockType::kAbusive;
  }
  return PopupBlockType::kNotBlocked;
}

// Tries to get the opener from either the |params| or |open_url_params|,
// otherwise uses the focused frame from |web_contents| as a proxy.
content::RenderFrameHost* GetSourceFrameForPopup(
    PopupNavigationDelegate* params,
    const content::OpenURLParams* open_url_params,
    content::WebContents* web_contents) {
  if (params->GetOpener())
    return params->GetOpener();
  // Make sure the source render frame host is alive before we attempt to
  // retrieve it from |open_url_params|.
  if (open_url_params) {
    content::RenderFrameHost* source = content::RenderFrameHost::FromID(
        open_url_params->source_render_frame_id,
        open_url_params->source_render_process_id);
    if (source)
      return source;
  }
  // The focused frame is not always the frame initiating the popup navigation
  // and is used as a fallback in case opener information is not available.
  return web_contents->GetFocusedFrame();
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

  // AddBlockedPopup() takes ownership of the delegate, so grab the source frame
  // first.
  content::RenderFrameHost* source_frame =
      GetSourceFrameForPopup(delegate.get(), open_url_params, web_contents);
  popup_blocker->AddBlockedPopup(std::move(delegate), window_features,
                                 block_type);
  auto* trigger = safe_browsing::AdPopupTrigger::FromWebContents(web_contents);
  if (trigger) {
    trigger->PopupWasBlocked(source_frame);
  }
  return nullptr;
}

}  // namespace blocked_content
