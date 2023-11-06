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
    const ToolbarController::ResponsiveElementInfoMap& element_info_map,
    int element_flex_order_start,
    views::View* toolbar_container_view,
    views::View* overflow_button)
    : element_ids_(element_ids),
      element_info_map_(element_info_map),
      element_flex_order_start_(element_flex_order_start),
      toolbar_container_view_(toolbar_container_view),
      overflow_button_(overflow_button) {
  if (ToolbarControllerUtil::PreventOverflow()) {
    return;
  }

  for (ui::ElementIdentifier id : element_ids) {
    auto* const toolbar_element =
        FindToolbarElementWithId(toolbar_container_view_, id);
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

    // Check `element_info_map` is constructed correctly i.e.
    // 1. keys should be a super set of `element_ids_`,
    // 2. ResponsiveElementInfo::activate_identifier is non-null.
    auto it = element_info_map.find(id);
    CHECK(it != element_info_map.end());
    CHECK(it->second.activate_identifier);

    // Create pop out state and pop out handlers to support pop out.
    if (it->second.observed_identifier.has_value()) {
      auto state = std::make_unique<PopOutState>();
      if (original_spec) {
        state->original_spec =
            absl::optional<views::FlexSpecification>(*original_spec);
      }
      state->responsive_spec = flex_spec;
      state->handler = std::make_unique<PopOutHandler>(
          this,
          views::ElementTrackerViews::GetContextForView(toolbar_container_view),
          id, it->second.observed_identifier.value());
      pop_out_state_[id] = std::move(state);
    }
  }
}

ToolbarController::~ToolbarController() = default;

ToolbarController::ResponsiveElementInfoMap
ToolbarController::GetDefaultElementInfoMap() {
  // TODO(crbug.com/1445573): Fill in observed identifier.
  return ToolbarController::ResponsiveElementInfoMap(
      {{kToolbarExtensionsContainerElementId,
        {IDS_OVERFLOW_MENU_ITEM_TEXT_EXTENSIONS,
         kExtensionsMenuButtonElementId}},
       {kToolbarSidePanelContainerElementId,
        {IDS_OVERFLOW_MENU_ITEM_TEXT_SIDE_PANEL,
         kToolbarSidePanelButtonElementId}},
       {kToolbarHomeButtonElementId,
        {IDS_OVERFLOW_MENU_ITEM_TEXT_HOME, kToolbarHomeButtonElementId}},
       {kToolbarChromeLabsButtonElementId,
        {IDS_OVERFLOW_MENU_ITEM_TEXT_LABS, kToolbarChromeLabsButtonElementId,
         kToolbarChromeLabsBubbleElementId}},
       {kToolbarMediaButtonElementId,
        {IDS_OVERFLOW_MENU_ITEM_TEXT_MEDIA_CONTROLS,
         kToolbarMediaButtonElementId, kToolbarMediaBubbleElementId}},
       {kToolbarDownloadButtonElementId,
        {IDS_OVERFLOW_MENU_ITEM_TEXT_DOWNLOADS, kToolbarDownloadButtonElementId,
         kToolbarDownloadBubbleElementId}},
       {kToolbarForwardButtonElementId,
        {IDS_OVERFLOW_MENU_ITEM_TEXT_FORWARD, kToolbarForwardButtonElementId}},
       {kToolbarAvatarButtonElementId,
        {IDS_OVERFLOW_MENU_ITEM_TEXT_PROFILE, kToolbarAvatarButtonElementId,
         kToolbarAvatarBubbleElementId}},
       {kToolbarNewTabButtonElementId,
        {IDS_OVERFLOW_MENU_ITEM_TEXT_NEW_TAB, kToolbarNewTabButtonElementId}}});
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

bool ToolbarController::ShouldShowOverflowButton() {
  // Once at least one button has been dropped by layout manager show overflow
  // button.
  for (ui::ElementIdentifier id : element_ids_) {
    if (IsOverflowed(id)) {
      return true;
    }
  }
  return false;
}

std::u16string ToolbarController::GetMenuText(ui::ElementIdentifier id) {
  return l10n_util::GetStringUTF16(element_info_map_.at(id).menu_text_id);
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

std::vector<ui::ElementIdentifier> ToolbarController::GetOverflowedElements() {
  std::vector<ui::ElementIdentifier> overflowed_buttons;
  if (ToolbarControllerUtil::PreventOverflow()) {
    return overflowed_buttons;
  }
  for (ui::ElementIdentifier id : element_ids_) {
    if (IsOverflowed(id)) {
      overflowed_buttons.push_back(id);
    }
  }
  return overflowed_buttons;
}

bool ToolbarController::IsOverflowed(ui::ElementIdentifier id) {
  const auto* const toolbar_element =
      FindToolbarElementWithId(toolbar_container_view_, id);
  const views::FlexLayout* flex_layout = static_cast<views::FlexLayout*>(
      toolbar_container_view_->GetLayoutManager());
  return flex_layout->CanBeVisible(toolbar_element) &&
         !toolbar_element->GetVisible();
}

std::unique_ptr<ui::SimpleMenuModel>
ToolbarController::CreateOverflowMenuModel() {
  CHECK(overflow_button_->GetVisible());
  auto menu_model = std::make_unique<ui::SimpleMenuModel>(this);
  for (size_t i = 0; i < element_ids_.size(); ++i) {
    if (IsOverflowed(element_ids_[i])) {
      menu_model->AddItem(i, GetMenuText(element_ids_[i]));
    }
  }
  return menu_model;
}

ui::ElementIdentifier ToolbarController::GetHiddenElementOfCommandId(
    int command_id) const {
  return element_ids_.at(command_id);
}

bool ToolbarController::IsCommandIdEnabled(int command_id) const {
  const auto* const element = FindToolbarElementWithId(
      toolbar_container_view_, element_ids_.at(command_id));
  return element->GetEnabled();
}

void ToolbarController::ExecuteCommand(int command_id, int event_flags) {
  ui::ElementIdentifier activate_identifier =
      element_info_map_.at(GetHiddenElementOfCommandId(command_id))
          .activate_identifier;
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
