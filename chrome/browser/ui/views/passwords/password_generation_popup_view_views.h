// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_GENERATION_POPUP_VIEW_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_GENERATION_POPUP_VIEW_VIEWS_H_

#include "base/macros.h"
#include "chrome/browser/ui/passwords/password_generation_popup_view.h"
#include "chrome/browser/ui/views/autofill/autofill_popup_base_view.h"

class PasswordGenerationPopupController;

namespace views {
class Label;
}

class PasswordGenerationPopupViewViews : public autofill::AutofillPopupBaseView,
                                         public PasswordGenerationPopupView {
 public:
  PasswordGenerationPopupViewViews(
      PasswordGenerationPopupController* controller,
      views::Widget* parent_widget);

  // PasswordGenerationPopupView implementation
  void Show() override;
  void Hide() override;
  void UpdateState() override;
  void UpdatePasswordValue() override;
  void UpdateBoundsAndRedrawPopup() override;
  void PasswordSelectionUpdated() override;

 private:
  class GeneratedPasswordBox;
  ~PasswordGenerationPopupViewViews() override;

  // Creates all the children views and adds them into layout.
  void CreateLayoutAndChildren();

  // views:Views implementation.
  void OnThemeChanged() override;
  void OnPaint(gfx::Canvas* canvas) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  gfx::Size CalculatePreferredSize() const override;

  // Sub view that displays the actual generated password.
  GeneratedPasswordBox* password_view_ = nullptr;

  // The footer label.
  views::Label* help_label_ = nullptr;

  // Controller for this view. Weak reference.
  PasswordGenerationPopupController* controller_;

  DISALLOW_COPY_AND_ASSIGN(PasswordGenerationPopupViewViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_GENERATION_POPUP_VIEW_VIEWS_H_
