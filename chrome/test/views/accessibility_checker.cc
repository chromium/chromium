// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/views/accessibility_checker.h"

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/widget/native_widget_delegate.h"
#include "ui/views/widget/widget.h"

namespace {

using ax::mojom::NameFrom;
using ax::mojom::Role;
using ax::mojom::State;
using ax::mojom::StringAttribute;

// Return helpful string for identifying a view.
// Includes the view class of every view in the ancestor chain, root first.
// Also provides the id.
// For example:
// BrowserView > OmniboxView (id 3).
std::string GetViewDebugString(const views::View* view) {
  // Get classes of ancestors.
  std::vector<std::string> classes;
  for (const views::View* ancestor = view; ancestor;
       ancestor = ancestor->parent())
    classes.insert(classes.begin(), ancestor->GetClassName());

  return base::JoinString(classes, " > ") +
         base::StringPrintf(" (id %d)", view->GetID());
}

bool DoesViewHaveAccessibleNameOrLabelError(ui::AXNodeData* data) {
  // Focusable nodes must have an accessible name, otherwise screen reader users
  // will not know what they landed on. For example, the reload button should
  // have an accessible name of "Reload".
  // Exceptions:
  // 1) Textfields can set the placeholder string attribute.
  // 2) Explicitly setting the name to "" is allowed if the view uses
  // AXNodedata.SetNameExplicitlyEmpty().

  // It has a name, we're done.
  if (!data->GetStringAttribute(StringAttribute::kName).empty())
    return false;

  // Text fields are allowed to have a placeholder instead.
  if (data->role == Role::kTextField &&
      !data->GetStringAttribute(StringAttribute::kPlaceholder).empty())
    return false;

  // Finally, a view is allowed to explicitly state that it has no name.
  if (data->GetNameFrom() == NameFrom::kAttributeExplicitlyEmpty)
    return false;

  // Has an error -- no name or placeholder, and not explicitly empty.
  return true;
}

bool DoesViewHaveAccessibilityErrors(views::View* view,
                                     std::string* error_message) {
  views::ViewAccessibility& view_accessibility = view->GetViewAccessibility();
  ui::AXNodeData node_data;
  // Get accessible node data from view_accessibility instead of view, because
  // some additional fields are processed and set there.
  view_accessibility.GetAccessibleNodeData(&node_data);

  std::string violations;

  // No checks for unfocusable items yet.
  if (node_data.HasState(State::kFocusable)) {
    if (DoesViewHaveAccessibleNameOrLabelError(&node_data)) {
      violations +=
          "\n- Focusable View has no accessible name or placeholder, and the "
          "name attribute does not use kAttributeExplicitlyEmpty.";
    }
    if (node_data.HasState(State::kInvisible))
      violations += "\n- Focusable View should not be invisible.";
  }

  if (violations.empty())
    return false;  // No errors.

  *error_message =
      "The following view violates DoesViewHaveAccessibilityErrors() when its "
      "widget becomes " +
      std::string(view->GetWidget()->IsVisible() ? "visible:\n" : "hidden:\n") +
      GetViewDebugString(view) + violations +
      "\n\nNote: for a more useful error message that includes a stack of how "
      "this view was constructed, use git cl patch 963284. Please leave a note "
      "on that CL if you find it useful.";
  return true;
}

bool DoesViewHaveAccessibilityErrorsRecursive(views::View* view,
                                              std::string* error_message) {
  const auto recurse = [error_message](auto* v) {
    return DoesViewHaveAccessibilityErrorsRecursive(v, error_message);
  };
  return DoesViewHaveAccessibilityErrors(view, error_message) ||
         std::any_of(view->children().begin(), view->children().end(), recurse);
}

}  // namespace

void AddFailureOnWidgetAccessibilityError(views::Widget* widget) {
  std::string error_message;
  if (widget->widget_delegate() && !widget->IsClosed() &&
      widget->GetRootView() &&
      DoesViewHaveAccessibilityErrorsRecursive(widget->GetRootView(),
                                               &error_message)) {
    ADD_FAILURE() << error_message;
  }
}

AccessibilityChecker::AccessibilityChecker() : scoped_observer_(this) {}

AccessibilityChecker::~AccessibilityChecker() {
  DCHECK(!scoped_observer_.IsObservingSources());
}

void AccessibilityChecker::OnBeforeWidgetInit(
    views::Widget::InitParams* params,
    views::internal::NativeWidgetDelegate* delegate) {
  ChromeViewsDelegate::OnBeforeWidgetInit(params, delegate);
  views::Widget* widget = delegate->AsWidget();
  if (widget)
    scoped_observer_.Add(widget);
}

void AccessibilityChecker::OnWidgetDestroying(views::Widget* widget) {
  scoped_observer_.Remove(widget);
}

void AccessibilityChecker::OnWidgetVisibilityChanged(views::Widget* widget,
                                                     bool visible) {
  // Test widget for accessibility errors both as it becomes visible or hidden,
  // in order to catch more errors. For example, to catch errors in the download
  // shelf we must check the browser window as it is hidden, because the shelf
  // is not visible when the browser window first appears.
  AddFailureOnWidgetAccessibilityError(widget);
}
