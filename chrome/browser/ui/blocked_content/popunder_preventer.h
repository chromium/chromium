// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BLOCKED_CONTENT_POPUNDER_PREVENTER_H_
#define CHROME_BROWSER_UI_BLOCKED_CONTENT_POPUNDER_PREVENTER_H_

#include <vector>

#include "base/memory/weak_ptr.h"

namespace content {
class WebContents;
}  // namespace content

// An object to block creation of pop-unders.
//
// This must be used whenever a window is activated. To use it, simply create an
// instance of PopunderPreventer *before* a WebContents is activated, and pass
// to the constructor the WebContents that is about to be activated. Additional
// activations of the same WebContents should call WillActivateWebContents().
class PopunderPreventer {
 public:
  explicit PopunderPreventer(content::WebContents* activating_contents);

  PopunderPreventer(const PopunderPreventer&) = delete;
  PopunderPreventer& operator=(const PopunderPreventer&) = delete;

  ~PopunderPreventer();

  // Clients should call this before the WebContents is activated additional
  // times during prolonged PopunderPreventer lifetimes. This records any other
  // active popup that should regain activation when the preventer is destroyed.
  void WillActivateWebContents(content::WebContents* activating_contents);

  // Clients can call this when a potential popunder is discovered, even if it
  // does not currently have activation; qualifying popups added here will
  // regain activation when the preventer is destroyed.
  void AddPotentialPopunder(content::WebContents* popup);

 private:
  std::vector<base::WeakPtr<content::WebContents>> popups_;
  base::WeakPtr<content::WebContents> activating_contents_;
};

#endif  // CHROME_BROWSER_UI_BLOCKED_CONTENT_POPUNDER_PREVENTER_H_
