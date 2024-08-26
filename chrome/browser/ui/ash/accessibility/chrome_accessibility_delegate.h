// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ACCESSIBILITY_CHROME_ACCESSIBILITY_DELEGATE_H_
#define CHROME_BROWSER_UI_ASH_ACCESSIBILITY_CHROME_ACCESSIBILITY_DELEGATE_H_

#include "ash/accessibility/accessibility_delegate.h"

// See ash::AccessibilityDelegate for details.
class ChromeAccessibilityDelegate : public ash::AccessibilityDelegate {
 public:
  ChromeAccessibilityDelegate();

  ChromeAccessibilityDelegate(const ChromeAccessibilityDelegate&) = delete;
  ChromeAccessibilityDelegate& operator=(const ChromeAccessibilityDelegate&) =
      delete;

  ~ChromeAccessibilityDelegate() override;

  // ash::AccessibilityDelegate:
  void SetMagnifierEnabled(bool enabled) override;
  bool IsMagnifierEnabled() const override;
  bool ShouldShowAccessibilityMenu() const override;
  void SaveScreenMagnifierScale(double scale) override;
  double GetSavedScreenMagnifierScale() override;
};

#endif  // CHROME_BROWSER_UI_ASH_ACCESSIBILITY_CHROME_ACCESSIBILITY_DELEGATE_H_
