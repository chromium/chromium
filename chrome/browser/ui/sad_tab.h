// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAD_TAB_H_
#define CHROME_BROWSER_UI_SAD_TAB_H_

#include <vector>

#include "base/process/kill.h"
#include "build/build_config.h"
#include "chrome/browser/ui/sad_tab_types.h"

namespace content {
class WebContents;
}

// Cross-platform interface to show the Sad tab UI.
class SadTab {
 public:
  enum class Action {
    BUTTON,
    HELP_LINK,
  };

  // Factory function to create the platform specific implementations.
  static SadTab* Create(content::WebContents* web_contents, SadTabKind kind);

  // Returns true if the sad tab should be shown.
  static bool ShouldShow(base::TerminationStatus status);

  virtual ~SadTab() {}

  // Called when the sad tab needs to be reinstalled in its window,
  // for example because an inactive tab was activated, or because a tab was
  // dragged to a new browser window.
  virtual void ReinstallInWebView() {}

  // These functions return resource string IDs for UI text. They may be
  // different for each sad tab. (Right now, the first sad tab in a session
  // suggests reloading and subsequent ones suggest sending feedback.)
  int GetTitle();
  int GetInfoMessage();
  int GetErrorCodeFormatString();
  int GetButtonTitle();
  int GetHelpLinkTitle();

  // Returns the resource string IDs for bullet points or empty vector if no
  // bullet points should be displayed.
  std::vector<int> GetSubMessages();

  // Returns the target of the "Learn more" link.
  const char* GetHelpLinkURL();

  // Returns the error code describing the reason for the crash.
  int GetCrashedErrorCode();

  // Virtual for testing.
  virtual void RecordFirstPaint();
  virtual void PerformAction(Action);

 protected:
  SadTab(content::WebContents* web_contents, SadTabKind kind);

  content::WebContents* web_contents() const { return web_contents_; }

 private:
  content::WebContents* web_contents_;
  SadTabKind kind_;
  // True if a crash happened in the last ten seconds. Repeated crashes
  // may suggest additional troubleshooting steps.
  bool is_repeatedly_crashing_;
  // True if repeatedly crashing and the browser is Google Chrome branded.
  bool show_feedback_button_;
  bool recorded_paint_;

  DISALLOW_COPY_AND_ASSIGN(SadTab);
};

#endif  // CHROME_BROWSER_UI_SAD_TAB_H_
