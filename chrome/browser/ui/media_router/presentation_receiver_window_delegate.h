// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_MEDIA_ROUTER_PRESENTATION_RECEIVER_WINDOW_DELEGATE_H_
#define CHROME_BROWSER_UI_MEDIA_ROUTER_PRESENTATION_RECEIVER_WINDOW_DELEGATE_H_

namespace content {
class WebContents;
}

// This interface allows communication between a PresentationReceiverWindow and
// its controller.  The controller provides a WebContents instance for the
// window, and the window can tell the controller when it is closing.
class PresentationReceiverWindowDelegate {
 public:
  // WebContents instance provided by the delegate to be displayed in the
  // window.
  virtual content::WebContents* web_contents() const = 0;

  // Notifies the delegate that the window is closed so it can perform any
  // necessary cleanup.
  virtual void WindowClosed() = 0;

 protected:
  ~PresentationReceiverWindowDelegate() = default;
};

#endif  // CHROME_BROWSER_UI_MEDIA_ROUTER_PRESENTATION_RECEIVER_WINDOW_DELEGATE_H_
