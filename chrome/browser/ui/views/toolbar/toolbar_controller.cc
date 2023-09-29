// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_controller.h"

#include "chrome/browser/ui/views/toolbar/overflow_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view_class_properties.h"

namespace {

std::u16string GenerateMenuText(const views::View* element) {
  // TODO(crbug.com/1481273): Menu items might deserve their own text
  // instead of using accessible name.
  std::u16string accessible_name = element->GetAccessibleName();
  if (!accessible_name.empty()) {
    return accessible_name;
  }

  ui::ElementIdentifier id = element->GetProperty(views::kElementIdentifierKey);
  CHECK(id);

  // Containers have no accessible names. Hard code their texts here.
  // TODO(crbug.com/1481273): Explore a more maintainable way to map container
  // id to text.
  if (id == kToolbarExtensionsContainerElementId) {
    return u"Extensions";
  } else if (id == kToolbarSidePanelContainerElementId) {
    return u"Side panel";
  }

  // Buttons with an empty accessible name should raise an error.
  NOTREACHED_NORETURN();
}

}  // namespace

ToolbarController::PopOutState::PopOutState() = default;
ToolbarController::PopOutState::~PopOutState() = default;

ToolbarController::PopOutHandler::PopOutHandler(
    ToolbarController* controller,
    ui::ElementContext context,
    ui::ElementIdentifier identifier,
    ui::ElementIdentifier observed_identifier)
    : controller_(controller),
      identifier_(identifier),
      observed_identifier_(observed_identifier) {
  shown_subscription_ =
      ui::ElementTracker::GetElementTracker()->AddElementShownCallback(
          observed_identifier_, context,
          base::BindRepeating(&PopOutHandler::OnElementShown,
                              base::Unretained(this)));
  hidden_subscription_ =
      ui::ElementTracker::GetElementTracker()->AddElementHiddenCallback(
          observed_identifier_, context,
          base::BindRepeating(&PopOutHandler::OnElementHidden,
                              base::Unretained(this)));
}

ToolbarController::PopOutHandler::~PopOutHandler() = default;

void ToolbarController::PopOutHandler::OnElementShown(
    ui::TrackedElement* element) {
  controller_->PopOut(identifier_);
}

void ToolbarController::PopOutHandler::OnElementHidden(
    ui::TrackedElement* element) {
  controller_->EndPopOut(identifier_);
}

ToolbarController::ToolbarController(
    std::vector<ui::ElementIdentifier> element_ids,
    PopOutIdentifierMap pop_out_identifier_map,
    int element_flex_order_start,
    views::View* toolbar_container_view,
    views::View* overflow_button)
    : element_ids_(element_ids),
      element_flex_order_start_(element_flex_order_start),
      toolbar_container_view_(toolbar_container_view),
      overflow_button_(overflow_button) {
  for (ui::ElementIdentifier id : element_ids) {
    views::View* toolbar_element = FindToolbarElementWithId(id);
    if (!toolbar_element) {
      continue;
    }

    views::FlexSpecification* original_spec =
        toolbar_element->GetProperty(views::kFlexBehaviorKey);
    views::FlexSpecification flex_spec;
    if (!original_spec) {
      flex_spec = views::FlexSpecification(
          views::MinimumFlexSizeRule::kPreferredSnapToZero,
          views::MaximumFlexSizeRule::kPreferred);
      toolbar_element->SetProperty(views::kFlexBehaviorKey, flex_spec);
    }
    flex_spec = toolbar_element->GetProperty(views::kFlexBehaviorKey)
                    ->WithOrder(element_flex_order_start++);
    toolbar_element->SetProperty(views::kFlexBehaviorKey, flex_spec);

    // Create pop out state and pop out handlers to support pop out.
    auto it = pop_out_identifier_map.find(id);
    if (it != pop_out_identifier_map.end()) {
      auto state = std::make_unique<PopOutState>();
      if (original_spec) {
        state->original_spec =
            absl::optional<views::FlexSpecification>(*original_spec);
      }
      state->responsive_spec = flex_spec;
      state->handler = std::make_unique<PopOutHandler>(
          this,
          views::ElementTrackerViews::GetContextForView(toolbar_container_view),
          id, it->second);
      pop_out_state_[id] = std::move(state);
    }
  }
}

ToolbarController::~ToolbarController() = default;

bool ToolbarController::PopOut(ui::ElementIdentifier identifier) {
  views::View* element = FindToolbarElementWithId(identifier);
  if (!element) {
    LOG(ERROR) << "Cannot find toolbar element id: " << identifier;
    return false;
  }
  const auto it = pop_out_state_.find(identifier);
  if (it == pop_out_state_.end()) {
    LOG(ERROR) << "Cannot find pop out state for id:" << identifier;
    return false;
  }
  if (it->second->is_popped_out) {
    return false;
  }

  it->second->is_popped_out = true;

  auto& original = it->second->original_spec;

  if (original.has_value()) {
    element->SetProperty(views::kFlexBehaviorKey, original.value());
  } else {
    element->ClearProperty(views::kFlexBehaviorKey);
  }

  element->parent()->InvalidateLayout();
  return true;
}

bool ToolbarController::EndPopOut(ui::ElementIdentifier identifier) {
  views::View* element = FindToolbarElementWithId(identifier);
  if (!element) {
    LOG(ERROR) << "Cannot find toolbar element id: " << identifier;
    return false;
  }
  const auto it = pop_out_state_.find(identifier);
  if (it == pop_out_state_.end()) {
    LOG(ERROR) << "Cannot find pop out state for id:" << identifier;
    return false;
  }
  if (!it->second->is_popped_out) {
    return false;
  }

  it->second->is_popped_out = false;

  element->SetProperty(views::kFlexBehaviorKey, it->second->responsive_spec);
  element->parent()->InvalidateLayout();
  return true;
}

bool ToolbarController::ShouldShowOverflowButton() {
  // Once at least one button has been dropped by layout manager show overflow
  // button.
  return GetOverflowedElements().size() > 0;
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

std::vector<const views::View*> ToolbarController::GetOverflowedElements() {
  std::vector<const views::View*> overflowed_buttons;
  const views::FlexLayout* flex_layout = static_cast<views::FlexLayout*>(
      toolbar_container_view_->GetLayoutManager());
  for (ui::ElementIdentifier id : element_ids_) {
    const views::View* toolbar_element = FindToolbarElementWithId(id);
    if (flex_layout->CanBeVisible(toolbar_element) &&
        !toolbar_element->GetVisible()) {
      overflowed_buttons.push_back(toolbar_element);
    }
  }
  return overflowed_buttons;
}

std::unique_ptr<ui::SimpleMenuModel>
ToolbarController::CreateOverflowMenuModel() {
  CHECK(overflow_button_->GetVisible());
  auto menu_model = std::make_unique<ui::SimpleMenuModel>(this);
  int menu_id_start = 0;
  for (auto* toolbar_element : GetOverflowedElements()) {
    menu_model->AddItem(menu_id_start++, GenerateMenuText(toolbar_element));
  }
  return menu_model;
}

void ToolbarController::ExecuteCommand(int command_id, int event_flags) {}

