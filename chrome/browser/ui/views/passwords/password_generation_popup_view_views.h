// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_GENERATION_POPUP_VIEW_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_GENERATION_POPUP_VIEW_VIEWS_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/passwords/password_generation_popup_view.h"
#include "chrome/browser/ui/views/autofill/autofill_popup_base_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
class PasswordGenerationPopupController;

namespace views {
class StyledLabel;
}

class PasswordGenerationPopupViewViews : public autofill::AutofillPopupBaseView,
                                         public PasswordGenerationPopupView {
 public:
  METADATA_HEADER(PasswordGenerationPopupViewViews);

  PasswordGenerationPopupViewViews(
      base::WeakPtr<PasswordGenerationPopupController> controller,
      views::Widget* parent_widget);

  PasswordGenerationPopupViewViews(const PasswordGenerationPopupViewViews&) =
      delete;
  PasswordGenerationPopupViewViews& operator=(
      const PasswordGenerationPopupViewViews&) = delete;

  // PasswordGenerationPopupView implementation
  [[nodiscard]] bool Show() override;
  void Hide() override;
  void UpdateState() override;
  void UpdateGeneratedPasswordValue() override;
  [[nodiscard]] bool UpdateBoundsAndRedrawPopup() override;
  void PasswordSelectionUpdated() override;

#if defined(UNIT_TEST)
  // Returns true if a minimized version with just a warning icon is created
  // instead of the whole `password_view_`.
  bool IsPopupMinimized() const { return !password_view_; }
#endif

 private:
  class GeneratedPasswordBox;
  ~PasswordGenerationPopupViewViews() override;

  // Creates all the children views and adds them into layout.
  void CreateLayoutAndChildren();

  // Returns true if full generation popup with `password_view_` was created.
  // The absence of this view means that only the minimized version of the popup
  // was created (with just a warning icon signaling that the currently typed
  // password is weak and expanding to password strength indicator on hover).
  bool FullPopupVisible() const;

  // views:Views implementation.
  void OnThemeChanged() override;
  void OnPaint(gfx::Canvas* canvas) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  gfx::Size CalculatePreferredSize() const override;

  // Sub view that displays the actual generated password.
  raw_ptr<GeneratedPasswordBox, DanglingUntriaged> password_view_ = nullptr;

  // The footer label.
  raw_ptr<views::StyledLabel, DanglingUntriaged> help_styled_label_ = nullptr;

  // Controller for this view. Weak reference.
  base::WeakPtr<PasswordGenerationPopupController> controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_GENERATION_POPUP_VIEW_VIEWS_H_
