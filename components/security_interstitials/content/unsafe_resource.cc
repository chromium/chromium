// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/content/unsafe_resource.h"

#include "base/bind.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace security_interstitials {

namespace {

content::WebContents* GetWebContentsByFrameID(int render_process_id,
                                              int render_frame_id) {
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(render_process_id, render_frame_id);
  if (!render_frame_host)
    return nullptr;
  return content::WebContents::FromRenderFrameHost(render_frame_host);
}

}  // namespace

UnsafeResource::UnsafeResource()
    : is_subresource(false),
      is_subframe(false),
      threat_type(safe_browsing::SB_THREAT_TYPE_SAFE),
      threat_source(safe_browsing::ThreatSource::UNKNOWN) {}

UnsafeResource::UnsafeResource(
    const UnsafeResource& other) = default;

UnsafeResource::~UnsafeResource() {}

bool UnsafeResource::IsMainPageLoadBlocked() const {
  // Subresource hits cannot happen until after main page load is committed.
  if (is_subresource)
    return false;

  switch (threat_type) {
    // Client-side phishing/malware detection interstitials never block the main
    // frame load, since they happen after the page is finished loading.
    case safe_browsing::SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING:
    case safe_browsing::SB_THREAT_TYPE_URL_CLIENT_SIDE_MALWARE:
    // Malicious ad activity reporting happens in the background.
    case safe_browsing::SB_THREAT_TYPE_BLOCKED_AD_POPUP:
    case safe_browsing::SB_THREAT_TYPE_BLOCKED_AD_REDIRECT:
    // Ad sampling happens in the background.
    case safe_browsing::SB_THREAT_TYPE_AD_SAMPLE:
    // Chrome SAVED password reuse warning happens after the page is finished
    // loading.
    case safe_browsing::SB_THREAT_TYPE_SAVED_PASSWORD_REUSE:
    // Chrome GAIA signed in and syncing password reuse warning happens after
    // the page is finished loading.
    case safe_browsing::SB_THREAT_TYPE_SIGNED_IN_SYNC_PASSWORD_REUSE:
    // Chrome GAIA signed in and non-syncing password reuse warning happens
    // after the page is finished loading.
    case safe_browsing::SB_THREAT_TYPE_SIGNED_IN_NON_SYNC_PASSWORD_REUSE:
    // Enterprise password reuse warning happens after the page is finished
    // loading.
    case safe_browsing::SB_THREAT_TYPE_ENTERPRISE_PASSWORD_REUSE:
    // Suspicious site collection happens in the background
    case safe_browsing::SB_THREAT_TYPE_SUSPICIOUS_SITE:
      return false;

    default:
      break;
  }

  return true;
}

content::NavigationEntry*
UnsafeResource::GetNavigationEntryForResource() const {
  content::WebContents* web_contents = web_contents_getter.Run();
  if (!web_contents)
    return nullptr;
  // If a safebrowsing hit occurs during main frame navigation, the navigation
  // will not be committed, and the pending navigation entry refers to the hit.
  if (IsMainPageLoadBlocked())
    return web_contents->GetController().GetPendingEntry();
  // If a safebrowsing hit occurs on a subresource load, or on a main frame
  // after the navigation is committed, the last committed navigation entry
  // refers to the page with the hit. Note that there may concurrently be an
  // unrelated pending navigation to another site, so GetActiveEntry() would be
  // wrong.
  return web_contents->GetController().GetLastCommittedEntry();
}

// static
base::Callback<content::WebContents*(void)>
UnsafeResource::GetWebContentsGetter(
    int render_process_host_id,
    int render_frame_id) {
  return base::Bind(&GetWebContentsByFrameID, render_process_host_id,
                    render_frame_id);
}

}  // security_interstitials
