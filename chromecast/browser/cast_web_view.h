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
#include "chromecast/graphics/cast_window_manager.h"
#include "content/public/browser/bluetooth_chooser.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace chromecast {

class CastWindowManager;

using shell::VisibilityPriority;

// A simplified interface for loading and displaying WebContents in cast_shell.
class CastWebView {
 public:
  class Delegate : public CastWebContents::Delegate,
                   public shell::CastContentWindow::Delegate {
   public:
    // Called when there is console log output from web_contents.
    // Returning true indicates that the delegate handled the message.
    // If false is returned the default logging mechanism will be used.
    virtual bool OnAddMessageToConsoleReceived(
        content::WebContents* source,
        int32_t level,
        const base::string16& message,
        int32_t line_no,
        const base::string16& source_id) = 0;

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

  // The parameters used to create a CastWebView instance. Passed to
  // CastWebContentsManager::CreateWebView().
  struct CreateParams {
    // The delegate for the CastWebView. Must be non-null.
    Delegate* delegate = nullptr;

    // Parameters for creating the content window for this CastWebView.
    shell::CastContentWindow::CreateParams window_params;

    // Identifies the activity that is hosted by this CastWebView.
    std::string activity_id = "";

    // Whether this CastWebView has a transparent background.
    bool transparent = false;

    // Whether this CastWebView is granted media access.
    bool allow_media_access = false;

    // Enable development mode for this CastWebView. Whitelists certain
    // functionality for the WebContents, like remote debugging and debugging
    // interfaces.
    bool enabled_for_dev = false;

    // Enable/Force 720p resolution for this CastWebView instance.
    bool force_720p_resolution = false;

    // Whether this CastWebView should be managed by web ui window manager.
    bool managed = true;

    CreateParams();
  };

  CastWebView();
  virtual ~CastWebView();

  virtual shell::CastContentWindow* window() const = 0;

  virtual content::WebContents* web_contents() const = 0;

  // Navigates to |url|. The loaded page will be preloaded if MakeVisible has
  // not been called on the object.
  virtual void LoadUrl(GURL url) = 0;

  // Begins the close process for this page (ie. triggering document.onunload).
  // A consumer of the class can be notified when the process has been finished
  // via Delegate::OnPageStopped(). The page will be torn down after
  // |shutdown_delay| has elapsed, or sooner if required.
  virtual void ClosePage(const base::TimeDelta& shutdown_delay) = 0;

  // Adds the page to the window manager and makes it visible to the user if
  // |is_visible| is true. |z_order| determines how this window is layered in
  // relationt other windows (higher value == more foreground).
  virtual void InitializeWindow(CastWindowManager* window_manager,
                                CastWindowManager::WindowId z_order,
                                VisibilityPriority initial_priority) = 0;

  // Sets the activity context exposed to web view and content window. The exact
  // format of context is defined by each activity.
  virtual void SetContext(base::Value context) = 0;

  // Allows the page to be shown on the screen. The page cannot be shown on the
  // screen until this is called.
  virtual void GrantScreenAccess() = 0;

  // Prevents the page from being shown on the screen until GrantScreenAccess()
  // is called.
  virtual void RevokeScreenAccess() = 0;

  // Observer interface:
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  base::ObserverList<Observer>::Unchecked observer_list_;

  DISALLOW_COPY_AND_ASSIGN(CastWebView);
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_WEB_VIEW_H_
