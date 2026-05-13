// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SESSION_CRASHED_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SESSION_CRASHED_BUBBLE_VIEW_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "chrome/browser/ui/session_crashed_bubble.h"
#include "components/metrics/metrics_reporting_level.h"
#include "ui/base/interaction/element_identifier.h"

namespace ui {
class DialogModel;
}

class BrowserWindowInterface;

namespace views {
class BubbleDialogModelHost;
}

// SessionCrashedBubbleView shows a bubble allowing the user to restore the
// previous session. If metrics reporting is not enabled a checkbox is presented
// allowing the user to turn it on.
class SessionCrashedBubbleView : public SessionCrashedBubble {
 public:
  // A helper class that listens to browser removal event.
  class BrowserRemovalObserver;

  using ChangeMetricsReportingStateCallback =
      base::RepeatingCallback<void(metrics::MetricsReportingLevel level)>;

  // Creates and shows the session crashed bubble, with |skip_tab_checking|
  // indicating whether skip the tab checking, and |uma_opted_in_already|
  // indicating whether the user has already opted-in to UMA. It will be called
  // by ShowIfNotOffTheRecordProfile. It takes ownership of |browser_observer|.
  static void Show(std::unique_ptr<BrowserRemovalObserver> browser_observer,
                   bool skip_tab_checking,
                   bool uma_opted_in_already);

  static views::BubbleDialogModelHost* GetModelHostForTesting();

  static ui::ElementIdentifier GetUmaConsentCheckboxIdForTesting();

  static void SetChangeMetricsReportingStateCallbackForTesting(
      ui::DialogModel* model,
      ChangeMetricsReportingStateCallback callback);

 private:
  friend class SessionCrashedBubbleViewTest;
  friend class SessionCrashedBubbleViewRestructureTest;

  // Internal show method also used by SessionCrashedBubbleViewTest.
  // TODO(pbos): Mock conditions in test instead.
  static views::BubbleDialogModelHost* ShowBubble(
      BrowserWindowInterface* browser,
      bool offer_uma_optin);
};

#endif  // CHROME_BROWSER_UI_VIEWS_SESSION_CRASHED_BUBBLE_VIEW_H_
