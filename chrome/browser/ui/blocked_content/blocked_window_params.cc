// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/blocked_content/blocked_window_params.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom.h"
#include "url/gurl.h"

BlockedWindowParams::BlockedWindowParams(
    const GURL& target_url,
    const url::Origin& initiator_origin,
    content::SiteInstance* source_site_instance,
    const content::Referrer& referrer,
    const std::string& frame_name,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& features,
    bool user_gesture,
    bool opener_suppressed)
    : target_url_(target_url),
      initiator_origin_(initiator_origin),
      source_site_instance_(source_site_instance),
      referrer_(referrer),
      frame_name_(frame_name),
      disposition_(disposition),
      features_(features),
      user_gesture_(user_gesture),
      opener_suppressed_(opener_suppressed) {}

BlockedWindowParams::BlockedWindowParams(const BlockedWindowParams& other) =
    default;

BlockedWindowParams::~BlockedWindowParams() = default;

NavigateParams BlockedWindowParams::CreateNavigateParams(
    content::RenderProcessHost* opener_process,
    content::WebContents* web_contents) const {
  GURL popup_url(target_url_);
  opener_process->FilterURL(false, &popup_url);
  NavigateParams nav_params(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()), popup_url,
      ui::PAGE_TRANSITION_LINK);
  nav_params.initiator_origin = initiator_origin_;
  nav_params.source_site_instance = source_site_instance_;
  nav_params.referrer = referrer_;
  nav_params.frame_name = frame_name_;
  nav_params.source_contents = web_contents;
  nav_params.is_renderer_initiated = true;
  nav_params.window_action = NavigateParams::SHOW_WINDOW;
  nav_params.user_gesture = user_gesture_;
  nav_params.opened_by_another_window = !opener_suppressed_;
  nav_params.window_features = features_;
  nav_params.disposition = disposition_;

  return nav_params;
}
