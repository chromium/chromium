// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_GENERATION_POPUP_VIEW_TESTER_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_GENERATION_POPUP_VIEW_TESTER_VIEWS_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/passwords/password_generation_popup_view_tester.h"

class PasswordGenerationPopupViewViews;

class PasswordGenerationPopupViewTesterViews
    : public PasswordGenerationPopupViewTester {
 public:
  explicit PasswordGenerationPopupViewTesterViews(
      PasswordGenerationPopupViewViews* view);

  PasswordGenerationPopupViewTesterViews(
      const PasswordGenerationPopupViewTesterViews&) = delete;
  PasswordGenerationPopupViewTesterViews& operator=(
      const PasswordGenerationPopupViewTesterViews&) = delete;

  ~PasswordGenerationPopupViewTesterViews() override;

  void SimulateMouseMovementAt(const gfx::Point& point) override;

  bool IsPopupMinimized() const override;

 private:
  // Weak reference
  raw_ptr<PasswordGenerationPopupViewViews> view_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_GENERATION_POPUP_VIEW_TESTER_VIEWS_H_
