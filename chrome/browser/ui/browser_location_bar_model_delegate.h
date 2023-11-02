// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_LOCATION_BAR_MODEL_DELEGATE_H_
#define CHROME_BROWSER_UI_BROWSER_LOCATION_BAR_MODEL_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/toolbar/chrome_location_bar_model_delegate.h"

class Browser;

// Implementation of LocationBarModelDelegate which uses an instance of
// Browser in order to fulfil its duties.
class BrowserLocationBarModelDelegate : public ChromeLocationBarModelDelegate {
 public:
  explicit BrowserLocationBarModelDelegate(Browser* browser);

  BrowserLocationBarModelDelegate(const BrowserLocationBarModelDelegate&) =
      delete;
  BrowserLocationBarModelDelegate& operator=(
      const BrowserLocationBarModelDelegate&) = delete;

  ~BrowserLocationBarModelDelegate() override;

  // ChromeLocationBarModelDelegate:
  content::WebContents* GetActiveWebContents() const override;

 private:
  const raw_ptr<Browser> browser_;
};

#endif  // CHROME_BROWSER_UI_BROWSER_LOCATION_BAR_MODEL_DELEGATE_H_
