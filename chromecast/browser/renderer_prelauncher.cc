// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/renderer_prelauncher.h"

#include <utility>

#include "base/logging.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "ipc/ipc_message.h"

namespace chromecast {

RendererPrelauncher::RendererPrelauncher(
    content::BrowserContext* browser_context,
    const GURL& gurl)
    : browser_context_(browser_context),
      gurl_(gurl),
      rph_routing_id_(MSG_ROUTING_NONE) {}

RendererPrelauncher::~RendererPrelauncher() {
  if (rph_routing_id_ != MSG_ROUTING_NONE) {
    DCHECK(site_instance_);
    site_instance_->GetProcess()->RemoveRoute(rph_routing_id_);
  }
}

void RendererPrelauncher::Prelaunch() {
  DLOG(INFO) << "Prelaunching for: " << gurl_;
  site_instance_ = content::SiteInstance::CreateForURL(browser_context_, gurl_);
  content::RenderProcessHost* rph = site_instance_->GetProcess();
  rph_routing_id_ = rph->GetNextRoutingID();
  rph->AddRoute(rph_routing_id_, this);
  rph->Init();
}

bool RendererPrelauncher::IsForURL(const GURL& gurl) const {
  if (!site_instance())
    return gurl_ == gurl;
  return site_instance() == site_instance()->GetRelatedSiteInstance(gurl);
}

// We don't process any IPC messages, but we do register as an IPC receiver to
// keep the RenderProcessHost alive.
bool RendererPrelauncher::OnMessageReceived(const IPC::Message& message) {
  return false;
}

}  // namespace chromecast
