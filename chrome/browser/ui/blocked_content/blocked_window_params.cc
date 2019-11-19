// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/blocked_content/blocked_window_params.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

BlockedWindowParams::BlockedWindowParams(
    const GURL& target_url,
    const url::Origin& initiator_origin,
    const content::Referrer& referrer,
    const std::string& frame_name,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& features,
    bool user_gesture,
    bool opener_suppressed)
    : target_url_(target_url),
      initiator_origin_(initiator_origin),
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
    content::WebContents* web_contents) const {
  GURL popup_url(target_url_);
  web_contents->GetMainFrame()->GetProcess()->FilterURL(false, &popup_url);
  NavigateParams nav_params(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()), popup_url,
      ui::PAGE_TRANSITION_LINK);
  nav_params.initiator_origin = initiator_origin_;
  nav_params.referrer = referrer_;
  nav_params.frame_name = frame_name_;
  nav_params.source_contents = web_contents;
  nav_params.is_renderer_initiated = true;
  nav_params.window_action = NavigateParams::SHOW_WINDOW;
  nav_params.user_gesture = user_gesture_;
  nav_params.created_with_opener = !opener_suppressed_;
  nav_params.window_bounds = web_contents->GetContainerBounds();
  if (features_.has_x)
    nav_params.window_bounds.set_x(features_.x);
  if (features_.has_y)
    nav_params.window_bounds.set_y(features_.y);
  if (features_.has_width)
    nav_params.window_bounds.set_width(features_.width);
  if (features_.has_height)
    nav_params.window_bounds.set_height(features_.height);

  nav_params.disposition = disposition_;

  return nav_params;
}
