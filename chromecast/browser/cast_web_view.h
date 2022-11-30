// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_WEB_VIEW_H_
#define CHROMECAST_BROWSER_CAST_WEB_VIEW_H_

#include <cstdint>
#include <string>

#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "chromecast/browser/cast_content_window.h"
#include "chromecast/browser/cast_web_contents.h"
#include "chromecast/ui/mojom/ui_service.mojom.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

namespace chromecast {

class CastWebService;

// A simplified interface for loading and displaying WebContents in cast_shell.
class CastWebView {
 public:
  // When the unique_ptr is reset, the CastWebView may not necessarily be
  // destroyed. In some cases ownership will be passed to the CastWebService,
  // which eventually handles destruction.
  using Scoped =
      std::unique_ptr<CastWebView, std::function<void(CastWebView*)>>;

  CastWebView() = default;

  CastWebView(const CastWebView&) = delete;
  CastWebView& operator=(const CastWebView&) = delete;

  virtual ~CastWebView() = default;

  virtual CastContentWindow* window() const = 0;

  virtual content::WebContents* web_contents() const = 0;

  virtual CastWebContents* cast_web_contents() = 0;

  virtual base::TimeDelta shutdown_delay() const = 0;

  // Called when the owning handle to CastWebView is destroyed.
  virtual void OwnerDestroyed() = 0;

  void BindReceivers(
      mojo::PendingReceiver<mojom::CastWebContents> web_contents_receiver,
      mojo::PendingReceiver<mojom::CastContentWindow> window_receiver);
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_WEB_VIEW_H_
