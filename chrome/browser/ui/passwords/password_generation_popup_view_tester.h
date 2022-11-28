// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_PASSWORD_GENERATION_POPUP_VIEW_TESTER_H_
#define CHROME_BROWSER_UI_PASSWORDS_PASSWORD_GENERATION_POPUP_VIEW_TESTER_H_

#include <memory>

#include "ui/gfx/geometry/point.h"

class PasswordGenerationPopupView;

// Helps test a PasswordGenerationPopupView.
class PasswordGenerationPopupViewTester {
 public:
  static std::unique_ptr<PasswordGenerationPopupViewTester> For(
      PasswordGenerationPopupView* view);

  virtual ~PasswordGenerationPopupViewTester() {}

  virtual void SimulateMouseMovementAt(const gfx::Point& point) = 0;

  virtual bool IsPopupMinimized() const = 0;
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_PASSWORD_GENERATION_POPUP_VIEW_TESTER_H_
