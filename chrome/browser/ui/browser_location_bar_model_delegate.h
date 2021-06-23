// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_LOCATION_BAR_MODEL_DELEGATE_H_
#define CHROME_BROWSER_UI_BROWSER_LOCATION_BAR_MODEL_DELEGATE_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/ui/toolbar/chrome_location_bar_model_delegate.h"

class Browser;

// Implementation of LocationBarModelDelegate which uses an instance of
// Browser in order to fulfil its duties.
class BrowserLocationBarModelDelegate : public ChromeLocationBarModelDelegate {
 public:
  explicit BrowserLocationBarModelDelegate(Browser* browser);
  ~BrowserLocationBarModelDelegate() override;

  // ChromeLocationBarModelDelegate:
  content::WebContents* GetActiveWebContents() const override;

 private:
  Browser* const browser_;

  DISALLOW_COPY_AND_ASSIGN(BrowserLocationBarModelDelegate);
};

#endif  // CHROME_BROWSER_UI_BROWSER_LOCATION_BAR_MODEL_DELEGATE_H_
