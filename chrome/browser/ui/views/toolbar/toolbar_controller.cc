// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_controller.h"

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ui/toolbar_controller_util.h"
#include "chrome/browser/ui/views/toolbar/overflow_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view_class_properties.h"

namespace {
base::flat_map<ui::ElementIdentifier, int> CalculateFlexOrder(
    const std::vector<ui::ElementIdentifier>& elements_in_overflow_order,
    int element_flex_order_start) {
  base::flat_map<ui::ElementIdentifier, int> id_to_order_map;

  // Loop in reverse order to ensure the first element gets the largest flex
  // order and overflows the first.
  for (auto it = elements_in_overflow_order.rbegin();
       it != elements_in_overflow_order.rend(); ++it) {
    id_to_order_map[*it] = element_flex_order_start++;
  }

  return id_to_order_map;
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
    const std::vector<ToolbarController::ResponsiveElementInfo>&
        responsive_elements,
    const std::vector<ui::ElementIdentifier>& elements_in_overflow_order,
    int element_flex_order_start,
    views::View* toolbar_container_view,
    views::View* overflow_button)
    : responsive_elements_(responsive_elements),
      element_flex_order_start_(element_flex_order_start),
      toolbar_container_view_(toolbar_container_view),
      overflow_button_(overflow_button) {
  if (ToolbarControllerUtil::PreventOverflow()) {
    return;
  }

  const auto id_to_order_map =
      CalculateFlexOrder(elements_in_overflow_order, element_flex_order_start);
  for (const auto& element : responsive_elements) {
    auto* const toolbar_element = FindToolbarElementWithId(
        toolbar_container_view_, element.overflow_identifier);
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
    flex_spec =
        toolbar_element->GetProperty(views::kFlexBehaviorKey)
            ->WithOrder(id_to_order_map.at(element.overflow_identifier));
    toolbar_element->SetProperty(views::kFlexBehaviorKey, flex_spec);

    // Check `responsive_elements_` is constructed correctly i.e.
    // ResponsiveElementInfo::activate_identifier is non-null.
    CHECK(element.activate_identifier);

    // Create pop out state and pop out handlers to support pop out.
    if (element.observed_identifier.has_value()) {
      auto state = std::make_unique<PopOutState>();
      if (original_spec) {
        state->original_spec =
            absl::optional<views::FlexSpecification>(*original_spec);
      }
      state->responsive_spec = flex_spec;
      state->handler = std::make_unique<PopOutHandler>(
          this,
          views::ElementTrackerViews::GetContextForView(toolbar_container_view),
          element.overflow_identifier, element.observed_identifier.value());
      pop_out_state_[element.overflow_identifier] = std::move(state);
    }
  }
}

ToolbarController::~ToolbarController() = default;

std::vector<ToolbarController::ResponsiveElementInfo>
ToolbarController::GetDefaultResponsiveElements() {
  // TODO(crbug.com/1445573): Fill in observed identifier.
  // Order matters because it should match overflow menu order top to bottom.
  return std::vector<ToolbarController::ResponsiveElementInfo>(
      {{kToolbarForwardButtonElementId, IDS_OVERFLOW_MENU_ITEM_TEXT_FORWARD,
        kToolbarForwardButtonElementId},
       {kToolbarHomeButtonElementId, IDS_OVERFLOW_MENU_ITEM_TEXT_HOME,
        kToolbarHomeButtonElementId, /*is_section_end=*/true},
       {kToolbarChromeLabsButtonElementId, IDS_OVERFLOW_MENU_ITEM_TEXT_LABS,
        kToolbarChromeLabsButtonElementId, /*is_section_end=*/false,
        kToolbarChromeLabsBubbleElementId},
       {kToolbarMediaButtonElementId,
        IDS_OVERFLOW_MENU_ITEM_TEXT_MEDIA_CONTROLS,
        kToolbarMediaButtonElementId, /*is_section_end=*/false,
        kToolbarMediaBubbleElementId},
       {kToolbarDownloadButtonElementId, IDS_OVERFLOW_MENU_ITEM_TEXT_DOWNLOADS,
        kToolbarDownloadButtonElementId, /*is_section_end=*/false,
        kToolbarDownloadBubbleElementId},
       {kToolbarNewTabButtonElementId, IDS_OVERFLOW_MENU_ITEM_TEXT_NEW_TAB,
        kToolbarNewTabButtonElementId, /*is_section_end=*/true},
       {kToolbarAvatarButtonElementId, IDS_OVERFLOW_MENU_ITEM_TEXT_PROFILE,
        kToolbarAvatarButtonElementId, /*is_section_end=*/false,
        kToolbarAvatarBubbleElementId}});
}

std::vector<ui::ElementIdentifier>
ToolbarController::GetDefaultOverflowOrder() {
  return std::vector<ui::ElementIdentifier>(
      {kToolbarHomeButtonElementId, kToolbarChromeLabsButtonElementId,
       kToolbarMediaButtonElementId, kToolbarDownloadButtonElementId,
       kToolbarNewTabButtonElementId, kToolbarForwardButtonElementId,
       kToolbarAvatarButtonElementId});
}

// Every activate identifier should have an action name in order to emit
// metrics. Please update action names in actions.xml to match this map.
std::string ToolbarController::GetActionNameFromElementIdentifier(
    ui::ElementIdentifier identifier) {
  static const base::NoDestructor<
      base::flat_map<ui::ElementIdentifier, base::StringPiece>>
      identifier_to_action_name_map({
          {kToolbarAvatarButtonElementId, "AvatarButton"},
          {kToolbarChromeLabsButtonElementId, "ChromeLabsButton"},
          {kToolbarDownloadButtonElementId, "DownloadButton"},
          {kExtensionsMenuButtonElementId, "ExtensionsMenuButton"},
          {kToolbarForwardButtonElementId, "ForwardButton"},
          {kToolbarHomeButtonElementId, "HomeButton"},
          {kToolbarMediaButtonElementId, "MediaButton"},
          {kToolbarNewTabButtonElementId, "NewTabButton"},
          {kToolbarSidePanelButtonElementId, "SidePanelButton"},
      });

  const auto it = identifier_to_action_name_map->find(identifier);
  return it == identifier_to_action_name_map->end()
             ? std::string()
             : base::StrCat({"ResponsiveToolbar.OverflowMenuItemActivated.",
                             it->second});
}

bool ToolbarController::PopOut(ui::ElementIdentifier identifier) {
  auto* const element =
      FindToolbarElementWithId(toolbar_container_view_, identifier);

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
  auto* const element =
      FindToolbarElementWithId(toolbar_container_view_, identifier);

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

bool ToolbarController::ShouldShowOverflowButton() const {
  // Once at least one button has been dropped by layout manager show overflow
  // button.
  for (const auto& element : responsive_elements_) {
    if (IsOverflowed(element.overflow_identifier)) {
      return true;
    }
  }
  return false;
}

std::u16string ToolbarController::GetMenuText(
    const ResponsiveElementInfo& element_info) const {
  return l10n_util::GetStringUTF16(element_info.menu_text_id);
}

views::View* ToolbarController::FindToolbarElementWithId(
    views::View* view,
    ui::ElementIdentifier id) {
  if (!view) {
    return nullptr;
  }
  if (view->GetProperty(views::kElementIdentifierKey) == id) {
    return view;
  }
  for (auto* child : view->children()) {
    if (auto* result = FindToolbarElementWithId(child, id)) {
      return result;
    }
  }
  return nullptr;
}

std::vector<const ToolbarController::ResponsiveElementInfo*>
ToolbarController::GetOverflowedElements() {
  std::vector<const ToolbarController::ResponsiveElementInfo*>
      overflowed_buttons;
  if (ToolbarControllerUtil::PreventOverflow()) {
    return overflowed_buttons;
  }
  for (const auto& element : responsive_elements_) {
    if (IsOverflowed(element.overflow_identifier)) {
      overflowed_buttons.push_back(&element);
    }
  }
  return overflowed_buttons;
}

bool ToolbarController::IsOverflowed(ui::ElementIdentifier id) const {
  const auto* const toolbar_element =
      FindToolbarElementWithId(toolbar_container_view_, id);
  const views::FlexLayout* const flex_layout = static_cast<views::FlexLayout*>(
      toolbar_container_view_->GetLayoutManager());
  return flex_layout->CanBeVisible(toolbar_element) &&
         !toolbar_element->GetVisible();
}

std::unique_ptr<ui::SimpleMenuModel>
ToolbarController::CreateOverflowMenuModel() {
  CHECK(overflow_button_->GetVisible());
  auto menu_model = std::make_unique<ui::SimpleMenuModel>(this);

  // True if the separator belonging to previous section has not been added yet.
  bool pre_separator_pending = false;

  for (size_t i = 0; i < responsive_elements_.size(); ++i) {
    const auto& element = responsive_elements_[i];
    if (IsOverflowed(element.overflow_identifier)) {
      if (pre_separator_pending && menu_model->GetItemCount() > 0) {
        menu_model->AddSeparator(ui::NORMAL_SEPARATOR);
      }
      menu_model->AddItem(i, GetMenuText(element));
      pre_separator_pending = false;
    }
    if (element.is_section_end) {
      pre_separator_pending = true;
    }
  }
  return menu_model;
}

bool ToolbarController::IsCommandIdEnabled(int command_id) const {
  const auto* const element = FindToolbarElementWithId(
      toolbar_container_view_,
      responsive_elements_.at(command_id).overflow_identifier);
  return element->GetEnabled();
}

void ToolbarController::ExecuteCommand(int command_id, int event_flags) {
  ui::ElementIdentifier activate_identifier =
      responsive_elements_.at(command_id).activate_identifier;
  const auto* const element =
      FindToolbarElementWithId(toolbar_container_view_, activate_identifier);
  CHECK(element);
  const auto* button = AsViewClass<views::Button>(element);
  button->button_controller()->NotifyClick();

  std::string action_name =
      GetActionNameFromElementIdentifier(activate_identifier);
  if (!action_name.empty()) {
    base::RecordAction(base::UserMetricsAction(action_name.c_str()));
  }
}
