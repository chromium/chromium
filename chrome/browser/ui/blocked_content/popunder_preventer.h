// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BLOCKED_CONTENT_POPUNDER_PREVENTER_H_
#define CHROME_BROWSER_UI_BLOCKED_CONTENT_POPUNDER_PREVENTER_H_

#include "base/memory/weak_ptr.h"

namespace content {
class WebContents;
}  // namespace content

// An object to block creation of pop-unders.
//
// This must be used whenever a window is activated. To use it, simply
// create an instance of PopunderPreventer *before* a WebContents is activated,
// and pass to the constructor the WebContents that is about to be activated.
class PopunderPreventer {
 public:
  explicit PopunderPreventer(content::WebContents* activating_contents);

  PopunderPreventer(const PopunderPreventer&) = delete;
  PopunderPreventer& operator=(const PopunderPreventer&) = delete;

  ~PopunderPreventer();

 private:
  base::WeakPtr<content::WebContents> popup_;
};

#endif  // CHROME_BROWSER_UI_BLOCKED_CONTENT_POPUNDER_PREVENTER_H_
