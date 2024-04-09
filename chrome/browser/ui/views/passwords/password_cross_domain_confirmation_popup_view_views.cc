// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_cross_domain_confirmation_popup_view_views.h"

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/autofill/popup/popup_base_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/metadata/view_factory_internal.h"
#include "url/gurl.h"

PasswordCrossDomainConfirmationPopupViewViews::
    PasswordCrossDomainConfirmationPopupViewViews(
        base::WeakPtr<autofill::AutofillPopupViewDelegate> delegate,
        views::Widget* parent_widget)
    : autofill::PopupBaseView(delegate, parent_widget) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  SetBackground(
      views::CreateThemedSolidBackground(ui::kColorDropdownBackground));
  AddChildView(views::Builder<views::Label>()
                   .SetText(u"TODO(b/330303918): add proper content")
                   .Build());
  SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(20, 20)));
}

PasswordCrossDomainConfirmationPopupViewViews::
    ~PasswordCrossDomainConfirmationPopupViewViews() = default;

void PasswordCrossDomainConfirmationPopupViewViews::Hide() {
  weak_factory_.InvalidateWeakPtrs();

  // TODO(b/333505414): `DoHide()` must be at the end, as it does `delete this`
  // in PopupBaseView, reconsider this behaviour and remove this warning.
  DoHide();
}

void PasswordCrossDomainConfirmationPopupViewViews::Show() {
  DoShow();
}

// static
base::WeakPtr<PasswordCrossDomainConfirmationPopupView>
PasswordCrossDomainConfirmationPopupView::Show(
    base::WeakPtr<autofill::AutofillPopupViewDelegate> delegate,
    const GURL& domain,
    const std::u16string& password_origin,
    base::OnceClosure confirmation_callback,
    base::OnceClosure cancel_callback) {
  auto* view = new PasswordCrossDomainConfirmationPopupViewViews(
      delegate, /*parent_widget=*/views::Widget::GetTopLevelWidgetForNativeView(
          delegate->container_view()));
  view->Show();
  return view->GetWeakPtr();
}
