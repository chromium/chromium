// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/security_dialog_tracker.h"

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "ui/base/class_property.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace {

DEFINE_UI_CLASS_PROPERTY_KEY(views::View*, kIdentifierViewKey, nullptr)

DECLARE_ELEMENT_IDENTIFIER_VALUE(kSecuritySensitiveDialogIdentifier);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kSecuritySensitiveDialogIdentifier);

using IdentifierView = views::View;

std::unique_ptr<IdentifierView> MakeIdentifierView() {
  auto view = std::make_unique<IdentifierView>();
  view->SetProperty(views::kViewIgnoredByLayoutKey, true);
  view->SetProperty(views::kElementIdentifierKey,
                    kSecuritySensitiveDialogIdentifier);
  return view;
}

}  // namespace

namespace extensions {

// static
SecurityDialogTracker* SecurityDialogTracker::GetInstance() {
  static base::NoDestructor<SecurityDialogTracker> s_instance;
  return &*s_instance;
}

void SecurityDialogTracker::AddSecurityDialog(views::Widget* widget) {
  views::View* root_view = widget->GetRootView();
  if (root_view->GetProperty(kIdentifierViewKey)) {
    return;
  }

  // Add an invisible identifier view to the root view of the widget.
  root_view->SetProperty(kIdentifierViewKey,
                         root_view->AddChildView(MakeIdentifierView()));
}

void SecurityDialogTracker::RemoveSecurityDialog(views::Widget* widget) {
  views::View* root_view = widget->GetRootView();
  views::View* identifier_view = root_view->GetProperty(kIdentifierViewKey);
  if (!identifier_view) {
    return;
  }

  root_view->RemoveChildViewT(identifier_view);
  root_view->ClearProperty(kIdentifierViewKey);
}

bool SecurityDialogTracker::BrowserHasVisibleSecurityDialogs(
    Browser* browser) const {
  views::ElementTrackerViews::ViewList identifier_views =
      views::ElementTrackerViews::GetInstance()->GetAllMatchingViews(
          kSecuritySensitiveDialogIdentifier,
          browser->window()->GetElementContext());
  return std::any_of(identifier_views.begin(), identifier_views.end(),
                     [](views::View* identifier_view) {
                       views::Widget* dialog_widget =
                           identifier_view->GetWidget();
                       return dialog_widget && dialog_widget->IsVisible();
                     });
}

SecurityDialogTracker::SecurityDialogTracker() = default;
SecurityDialogTracker::~SecurityDialogTracker() = default;

}  // namespace extensions
