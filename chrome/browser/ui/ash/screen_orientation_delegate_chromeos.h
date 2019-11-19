// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SCREEN_ORIENTATION_DELEGATE_CHROMEOS_H_
#define CHROME_BROWSER_UI_ASH_SCREEN_ORIENTATION_DELEGATE_CHROMEOS_H_

#include "content/public/browser/screen_orientation_delegate.h"

// Chrome OS implementation for screen orientation JS api.
class ScreenOrientationDelegateChromeos
    : public content::ScreenOrientationDelegate {
 public:
  ScreenOrientationDelegateChromeos();
  ~ScreenOrientationDelegateChromeos() override;

 private:
  // content::ScreenOrientationDelegate:
  bool FullScreenRequired(content::WebContents* web_contents) override;
  void Lock(content::WebContents* web_contents,
            blink::WebScreenOrientationLockType lock_orientation) override;
  bool ScreenOrientationProviderSupported() override;
  void Unlock(content::WebContents* web_contents) override;

  DISALLOW_COPY_AND_ASSIGN(ScreenOrientationDelegateChromeos);
};

#endif  // CHROME_BROWSER_UI_ASH_SCREEN_ORIENTATION_DELEGATE_CHROMEOS_H_
