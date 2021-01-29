// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_GENERATION_POPUP_VIEW_TESTER_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_GENERATION_POPUP_VIEW_TESTER_VIEWS_H_

#include "base/macros.h"
#include "chrome/browser/ui/passwords/password_generation_popup_view_tester.h"

class PasswordGenerationPopupViewViews;

class PasswordGenerationPopupViewTesterViews
    : public PasswordGenerationPopupViewTester {
 public:
  explicit PasswordGenerationPopupViewTesterViews(
      PasswordGenerationPopupViewViews* view);
  ~PasswordGenerationPopupViewTesterViews() override;

  void SimulateMouseMovementAt(const gfx::Point& point) override;

 private:
  // Weak reference
  PasswordGenerationPopupViewViews* view_;

  DISALLOW_COPY_AND_ASSIGN(PasswordGenerationPopupViewTesterViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_GENERATION_POPUP_VIEW_TESTER_VIEWS_H_
