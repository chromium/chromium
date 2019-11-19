// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_WEB_VIEW_H_
#define CHROMECAST_BROWSER_CAST_WEB_VIEW_H_

#include <cstdint>
#include <string>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromecast/browser/cast_content_window.h"
#include "chromecast/browser/cast_web_contents.h"
#include "chromecast/ui/mojom/ui_service.mojom.h"
#include "content/public/browser/bluetooth_chooser.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace chromecast {

// A simplified interface for loading and displaying WebContents in cast_shell.
class CastWebView {
 public:
  class Delegate : public CastWebContents::Delegate,
                   public CastContentWindow::Delegate {
   public:
    // Invoked by CastWebView when WebContentsDelegate::RunBluetoothChooser is
    // called. Returns a BluetoothChooser, a class used to solicit bluetooth
    // device selection from the user for WebBluetooth applications. If a
    // delegate does not provide an implementation, WebBluetooth will not be
    // supported for that CastWebView.
    virtual std::unique_ptr<content::BluetoothChooser> RunBluetoothChooser(
        content::RenderFrameHost* frame,
        const content::BluetoothChooser::EventHandler& event_handler);
  };

  // Observer interface for tracking CastWebView lifetime.
  class Observer {
   public:
    // Notifies that |web_view| is being destroyed. |web_view| should be assumed
    // invalid after this method returns.
    virtual void OnPageDestroyed(CastWebView* web_view) {}

   protected:
    virtual ~Observer() {}
  };

  // When the unique_ptr is reset, the CastWebView may not necessarily be
  // destroyed. In some cases ownership will be passed to the CastWebService,
  // which eventually handles destruction.
  using Scoped =
      std::unique_ptr<CastWebView, std::function<void(CastWebView*)>>;

  // The parameters used to create a CastWebView instance. Passed to
  // CastWebService::CreateWebView().
  struct CreateParams {
    // The delegate for the CastWebView. Must be non-null. If the delegate is
    // destroyed before CastWebView, the WeakPtr will be invalidated on the main
    // UI thread.
    base::WeakPtr<Delegate> delegate = nullptr;

    // Parameters for initializing CastWebContents. These will be passed as-is
    // to a CastWebContents instance, which should be used by all CastWebView
    // implementations.
    CastWebContents::InitParams web_contents_params;

    // Parameters for creating the content window for this CastWebView.
    CastContentWindow::CreateParams window_params;

    // Identifies the activity that is hosted by this CastWebView.
    std::string activity_id = "";

    // Sdk version of the application (if available) hosted by this CastWebView.
    std::string sdk_version = "";

    // Whether this CastWebView is granted media access.
    bool allow_media_access = false;

    // Enable/Force 720p resolution for this CastWebView instance.
    bool force_720p_resolution = false;

    // Whether this CastWebView should be managed by web ui window manager.
    bool managed = true;

    // Prefix for JS console logs. This can be used to help identify the source
    // of console log messages.
    std::string log_prefix = "";

    // Delays CastWebView deletion after CastWebView::Scoped is reset. The
    // default value is zero, which means the CastWebView will be deleted
    // immediately and synchronously.
    base::TimeDelta shutdown_delay = base::TimeDelta();

    CreateParams();
    CreateParams(const CreateParams& other);
    ~CreateParams();
  };

  explicit CastWebView(const CreateParams& create_params);
  virtual ~CastWebView();

  virtual CastContentWindow* window() const = 0;

  virtual content::WebContents* web_contents() const = 0;

  virtual CastWebContents* cast_web_contents() = 0;

  base::TimeDelta shutdown_delay() const { return shutdown_delay_; }

  // Navigates to |url|. The loaded page will be preloaded if MakeVisible has
  // not been called on the object.
  virtual void LoadUrl(GURL url) = 0;

  // Begins the close process for this page (ie. triggering document.onunload).
  // A consumer of the class can be notified when the process has been finished
  // via Delegate::OnPageStopped(). The page will be torn down after
  // |CreateParams::shutdown_delay| has elapsed, or immediately if the browser
  // is shutting down.
  virtual void ClosePage() = 0;

  // Closes the page immediately, ignoring |CreateParams::shutdown_delay|.
  void ForceClose();

  // Adds the page to the window manager and makes it visible to the user if
  // |is_visible| is true. |z_order| determines how this window is layered in
  // relationt other windows (higher value == more foreground).
  virtual void InitializeWindow(mojom::ZOrder z_order,
                                VisibilityPriority initial_priority) = 0;

  // Allows the page to be shown on the screen. The page cannot be shown on the
  // screen until this is called.
  virtual void GrantScreenAccess() = 0;

  // Prevents the page from being shown on the screen until GrantScreenAccess()
  // is called.
  virtual void RevokeScreenAccess() = 0;

  // Observer interface:
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  base::WeakPtr<Delegate> delegate_;

 private:
  base::TimeDelta shutdown_delay_;

  base::ObserverList<Observer>::Unchecked observer_list_;

  DISALLOW_COPY_AND_ASSIGN(CastWebView);
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_WEB_VIEW_H_
