// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_WEB_CONTENTS_CLOSE_HANDLER_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_WEB_CONTENTS_CLOSE_HANDLER_H_

#include "base/macros.h"
#include "base/timer/timer.h"

class WebContentsCloseHandlerDelegate;
class WebContentsCloseHandlerTest;

// WebContentsCloseHandler handles notifying its delegate at the right time
// to clone and/or destroy the layer tree of the active tab. This is done so
// that on closing a window the user sees the contents of the last active
// tab rather than an empty rect.
class WebContentsCloseHandler {
 public:
  explicit WebContentsCloseHandler(WebContentsCloseHandlerDelegate* delegate);
  ~WebContentsCloseHandler();

  // Invoked when a tab is inserted.
  void TabInserted();

  // Invoked when the active WebContents changes.
  void ActiveTabChanged();

  // Invoked when all the tabs are about to be closed.
  void WillCloseAllTabs();

  // Invoked when the close was canceled.
  void CloseAllTabsCanceled();

 private:
  friend class WebContentsCloseHandlerTest;

  // Invoked from the |timer_|. If hit it means enough time has expired after a
  // close was canceled.
  void OnStillHaventClosed();

  WebContentsCloseHandlerDelegate* delegate_;

  // If true, WillCloseAllTabs() has been invoked.
  bool in_close_;

  // Set to true if the active tab changes while closing (ActiveTabChanged()
  // was invoked following a WillCloseAllTabs()).
  bool tab_changed_after_clone_;

  base::OneShotTimer timer_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsCloseHandler);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_WEB_CONTENTS_CLOSE_HANDLER_H_
