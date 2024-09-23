// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_GENERATION_POPUP_VIEW_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_GENERATION_POPUP_VIEW_VIEWS_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/passwords/password_generation_popup_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_base_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/accessibility/view_accessibility.h"

class PasswordGenerationPopupController;

class PasswordGenerationPopupViewViews : public autofill::PopupBaseView,
                                         public PasswordGenerationPopupView {
  METADATA_HEADER(PasswordGenerationPopupViewViews, autofill::PopupBaseView)

 public:
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
  void NudgePasswordSelectionUpdated() override;

  const views::ViewAccessibility& GetPasswordViewViewAccessibilityForTest();
  const views::ViewAccessibility& GetAcceptButtonViewAccessibilityForTest();
  const views::ViewAccessibility& GetCancelButtonViewAccessibilityForTest();

 private:
  class GeneratedPasswordBox;
  ~PasswordGenerationPopupViewViews() override;

  // Creates all the children views and adds them into layout.
  void CreateLayoutAndChildren();

  // views:Views implementation.
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  // Helper function to update the expanded and collapsed accessible states of
  // the view.
  void UpdateExpandedCollapsedAccessibleState();

  // Helper function to update the invisible accessible state of the view.
  void UpdateInvisibleAccessibleState();

  // Sub view that displays the actual generated password.
  raw_ptr<GeneratedPasswordBox> password_view_ = nullptr;

  // Sub view that displays the nudge password buttons row.
  raw_ptr<views::View> nudge_password_buttons_view_ = nullptr;

  // Controller for this view. Weak reference.
  base::WeakPtr<PasswordGenerationPopupController> controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_GENERATION_POPUP_VIEW_VIEWS_H_
