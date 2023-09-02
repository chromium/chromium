// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_controller.h"

#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view_class_properties.h"

ToolbarController::ToolbarController(
    std::vector<ui::ElementIdentifier> element_ids,
    int element_flex_order_start,
    views::View* toolbar_container_view)
    : element_ids_(element_ids),
      element_flex_order_start_(element_flex_order_start),
      toolbar_container_view_(toolbar_container_view) {
  for (ui::ElementIdentifier id : element_ids_) {
    views::View* toolbar_element = FindToolbarElementWithId(id);
    if (!toolbar_element) {
      continue;
    }

    views::FlexSpecification flex_spec;
    if (!toolbar_element->GetProperty(views::kFlexBehaviorKey)) {
      flex_spec = views::FlexSpecification(
          views::MinimumFlexSizeRule::kPreferredSnapToZero,
          views::MaximumFlexSizeRule::kPreferred);
      toolbar_element->SetProperty(views::kFlexBehaviorKey, flex_spec);
    }
    flex_spec = toolbar_element->GetProperty(views::kFlexBehaviorKey)
                    ->WithOrder(element_flex_order_start++);
    toolbar_element->SetProperty(views::kFlexBehaviorKey, flex_spec);
  }
}

const views::View* ToolbarController::FindToolbarElementWithId(
    ui::ElementIdentifier id) const {
  const views::View::Views toolbar_elements =
      toolbar_container_view_->children();
  for (const views::View* element : toolbar_elements) {
    if (element->GetProperty(views::kElementIdentifierKey) == id) {
      return element;
    }
  }
  return nullptr;
}

ToolbarController::~ToolbarController() = default;
