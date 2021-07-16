// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_WEB_VIEW_H_
#define CHROMECAST_BROWSER_CAST_WEB_VIEW_H_

#include <cstdint>
#include <string>

#include "base/macros.h"
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
  class Delegate : public CastWebContents::Delegate,
                   public CastContentWindow::Delegate {};

  // When the unique_ptr is reset, the CastWebView may not necessarily be
  // destroyed. In some cases ownership will be passed to the CastWebService,
  // which eventually handles destruction.
  using Scoped =
      std::unique_ptr<CastWebView, std::function<void(CastWebView*)>>;

  // The parameters used to create a CastWebView instance. Passed to
  // CastWebService::CreateWebView(). All delegate WeakPtrs will be invalidated
  // on the main UI thread if they are destroyed before CastWebView.
  struct CreateParams {
    // CastWebView delegate. Must be non-null.
    base::WeakPtr<Delegate> delegate = nullptr;

    // CastWebContents delegate. This may be null.
    base::WeakPtr<CastWebContents::Delegate> web_contents_delegate = nullptr;

    // CastContentWindow delegate. This may be null.
    base::WeakPtr<CastContentWindow::Delegate> window_delegate = nullptr;

    CreateParams();
    CreateParams(const CreateParams& other);
    ~CreateParams();
  };

  CastWebView() = default;
  virtual ~CastWebView() = default;

  virtual CastContentWindow* window() const = 0;

  virtual content::WebContents* web_contents() const = 0;

  virtual CastWebContents* cast_web_contents() = 0;

  virtual base::TimeDelta shutdown_delay() const = 0;

  void BindReceivers(
      mojo::PendingReceiver<mojom::CastWebContents> web_contents_receiver,
      mojo::PendingReceiver<mojom::CastContentWindow> window_receiver);

 private:
  DISALLOW_COPY_AND_ASSIGN(CastWebView);
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_WEB_VIEW_H_
