// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_CHROME_ACCESSIBILITY_DELEGATE_H_
#define CHROME_BROWSER_UI_ASH_CHROME_ACCESSIBILITY_DELEGATE_H_

#include "ash/accessibility/accessibility_delegate.h"
#include "base/macros.h"

// See ash::AccessibilityDelegate for details.
class ChromeAccessibilityDelegate : public ash::AccessibilityDelegate {
 public:
  ChromeAccessibilityDelegate();
  ~ChromeAccessibilityDelegate() override;

  // ash::AccessibilityDelegate:
  void SetMagnifierEnabled(bool enabled) override;
  bool IsMagnifierEnabled() const override;
  bool ShouldShowAccessibilityMenu() const override;
  void SaveScreenMagnifierScale(double scale) override;
  double GetSavedScreenMagnifierScale() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeAccessibilityDelegate);
};

#endif  // CHROME_BROWSER_UI_ASH_CHROME_ACCESSIBILITY_DELEGATE_H_
