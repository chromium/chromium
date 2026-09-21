// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/security_dialog_tracker.h"

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "ui/base/class_property.h"
#include "ui/base/interaction/element_identifier.h"
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

void SecurityDialogTracker::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void SecurityDialogTracker::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void SecurityDialogTracker::AddSecurityDialog(views::Widget* widget) {
  views::View* root_view = widget->GetRootView();
  if (root_view->GetProperty(kIdentifierViewKey)) {
    return;
  }

  // Add an invisible identifier view to the root view of the widget.
  root_view->SetProperty(kIdentifierViewKey,
                         root_view->AddChildView(MakeIdentifierView()));

  for (Observer& observer : observers_) {
    observer.OnSecurityDialogAdded(widget);
  }
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

// static
bool SecurityDialogTracker::IsWidgetOrDescendantOf(
    const views::Widget* widget,
    const views::Widget* ancestor) {
  if (!widget || !ancestor) {
    return false;
  }
  for (const views::Widget* w = widget; w; w = w->parent()) {
    if (w == ancestor) {
      return true;
    }
  }
  return false;
}

bool SecurityDialogTracker::BrowserHasVisibleSecurityDialogs(
    BrowserWindowInterface* browser,
    const views::Widget* ignore_widget) const {
  if (!browser) {
    return false;
  }
  auto* browser_elements = BrowserElementsViews::From(browser);
  if (!browser_elements) {
    return false;
  }
  const auto views =
      browser_elements->GetAllViews(kSecuritySensitiveDialogIdentifier);
  return std::any_of(views.begin(), views.end(),
                     [ignore_widget](views::View* identifier_view) {
                       views::Widget* dialog_widget =
                           identifier_view->GetWidget();
                       if (!dialog_widget || !dialog_widget->IsVisible() ||
                           dialog_widget->IsClosed()) {
                         return false;
                       }
                       if (ignore_widget && IsWidgetOrDescendantOf(
                                                dialog_widget, ignore_widget)) {
                         return false;
                       }
                       return true;
                     });
}

SecurityDialogTracker::SecurityDialogTracker() = default;
SecurityDialogTracker::~SecurityDialogTracker() = default;

}  // namespace extensions
