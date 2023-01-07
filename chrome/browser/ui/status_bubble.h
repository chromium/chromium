// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STATUS_BUBBLE_H_
#define CHROME_BROWSER_UI_STATUS_BUBBLE_H_

#include <string>


class GURL;

////////////////////////////////////////////////////////////////////////////////
// StatusBubble interface
//  An interface implemented by an object providing the status display area of
//  the browser window.
//
class StatusBubble {
 public:
  // On hover, expand status bubble to fit long URL after this delay.
  static const int kExpandHoverDelayMS = 1600;

  virtual ~StatusBubble() {}

  // Sets the bubble contents to a specific string and causes the bubble
  // to display immediately. Subsequent empty SetURL calls (typically called
  // when the cursor exits a link) will set the status bubble back to its
  // status text. To hide the status bubble again, either call SetStatus
  // with an empty string, or call Hide().
  virtual void SetStatus(const std::u16string& status) = 0;

  // Sets the bubble text to a URL - if given a non-empty URL, this will cause
  // the bubble to fade in and remain open until given an empty URL or until
  // the Hide() method is called.
  virtual void SetURL(const GURL& url) = 0;

  // Skip the fade and instant-hide the bubble.
  virtual void Hide() = 0;

  // Called when the user's mouse has moved over web content. This is used to
  // determine when the status area should move out of the way of the user's
  // mouse. |left_content| is true if the mouse just left the content area.
  virtual void MouseMoved(bool left_content) = 0;

  // Called when the download shelf becomes visible or invisible.
  // This is used by to ensure that the status bubble does not obscure
  // the download shelf, when it is visible.
  virtual void UpdateDownloadShelfVisibility(bool visible) = 0;
};

#endif  // CHROME_BROWSER_UI_STATUS_BUBBLE_H_
