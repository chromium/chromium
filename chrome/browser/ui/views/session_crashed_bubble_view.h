// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SESSION_CRASHED_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SESSION_CRASHED_BUBBLE_VIEW_H_

class BrowserWindowInterface;

namespace views {
class BubbleDialogDelegate;
}

// SessionCrashedBubbleView shows a bubble allowing the user to restore the
// previous session. If metrics reporting is not enabled a checkbox is presented
// allowing the user to turn it on.
class SessionCrashedBubbleView {
 public:
  SessionCrashedBubbleView() = delete;

  static views::BubbleDialogDelegate* GetInstanceForTest();

 private:
  friend class SessionCrashedBubbleViewTest;

  // Internal show method also used by SessionCrashedBubbleViewTest.
  // TODO(pbos): Mock conditions in test instead.
  static views::BubbleDialogDelegate* ShowBubble(
      BrowserWindowInterface* browser,
      bool uma_opted_in_already,
      bool offer_uma_optin);
};

#endif  // CHROME_BROWSER_UI_VIEWS_SESSION_CRASHED_BUBBLE_VIEW_H_
