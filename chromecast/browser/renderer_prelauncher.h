// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_RENDERER_PRELAUNCHER_H_
#define CHROMECAST_BROWSER_RENDERER_PRELAUNCHER_H_

#include "base/memory/ref_counted.h"
#include "ipc/ipc_listener.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
class RenderProcessHost;
class SiteInstance;
}  // namespace content

namespace chromecast {

// This class pre-launches a renderer process for the SiteInstance of a URL,
// and retains a handle on its RenderProcessHost to keep it alive even when
// there is no WebContents using the renderer process.  To release the
// RenderProcessHost, this class must be deleted.
class RendererPrelauncher : private IPC::Listener {
 public:
  RendererPrelauncher(content::BrowserContext* browser_context,
                      const GURL& gurl);

  RendererPrelauncher(const RendererPrelauncher&) = delete;
  RendererPrelauncher& operator=(const RendererPrelauncher&) = delete;

  ~RendererPrelauncher() override;

  virtual void Prelaunch();
  bool IsForURL(const GURL& gurl) const;

  const GURL& url() const { return gurl_; }
  scoped_refptr<content::SiteInstance> site_instance() const {
    return site_instance_;
  }

 private:
  // IPC::Listener implementation:
  bool OnMessageReceived(const IPC::Message& message) override;

  content::BrowserContext* const browser_context_;
  scoped_refptr<content::SiteInstance> site_instance_;
  const GURL gurl_;
  int32_t rph_routing_id_;
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_RENDERER_PRELAUNCHER_H_
