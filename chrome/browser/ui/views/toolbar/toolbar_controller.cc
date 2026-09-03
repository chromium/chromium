// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_controller.h"

#include <optional>
#include <ranges>
#include <string_view>
#include <variant>
#include <vector>

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/side_panel/side_panel_action_callback.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/toolbar_controller_util.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/toolbar/overflow_button.h"
#include "chrome/browser/ui/views/toolbar/overflow_menu.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_button_status_indicator.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace {

base::flat_map<ui::ElementIdentifier, int> CalculateFlexOrder(
    const std::vector<ui::ElementIdentifier>& elements_in_overflow_order,
    int element_flex_order_start) {
  base::flat_map<ui::ElementIdentifier, int> id_to_order_map;

  // Loop in reverse order to ensure the first element gets the largest flex
  // order and overflows the first.
  for (auto it : std::views::reverse(elements_in_overflow_order)) {
    id_to_order_map[it] = element_flex_order_start++;
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
  controller_->PopOut(identifier_, /*show_synchronously=*/false);
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
    WebUIToolbarControllerDelegate* webui_toolbar_controller_delegate,
    OverflowButton* overflow_button,
    ToolbarController::PinnedActionsDelegate* pinned_actions_delegate,
    PinnedToolbarActionsModel* pinned_actions_model)
    : element_flex_order_start_(element_flex_order_start),
      toolbar_container_view_(toolbar_container_view),
      webui_toolbar_controller_delegate_(webui_toolbar_controller_delegate),
      overflow_button_(overflow_button),
      pinned_actions_delegate_(pinned_actions_delegate),
      pinned_actions_model_(pinned_actions_model),
      overflow_menu_(responsive_elements,
                     this,
                     pinned_actions_delegate,
                     pinned_actions_model) {
  if (ToolbarControllerUtil::PreventOverflow()) {
    return;
  }

  const auto id_to_order_map =
      CalculateFlexOrder(elements_in_overflow_order, element_flex_order_start);
  for (const auto& element : overflow_menu_.responsive_elements()) {
    const auto& overflow_id = element.overflow_id;

    std::visit(
        absl::Overload{
            [](actions::ActionId id) { return; },
            [&](ToolbarController::ElementIdInfo id) {
              auto* const toolbar_element = FindToolbarElementWithId(
                  toolbar_container_view_, id.overflow_identifier);
              if (!toolbar_element) {
                return;
              }

              views::FlexSpecification* original_spec =
                  toolbar_element->GetProperty(views::kFlexBehaviorKey);
              views::FlexSpecification flex_spec;
              if (!original_spec) {
                flex_spec = views::FlexSpecification(
                    views::LayoutOrientation::kHorizontal,
                    views::MinimumFlexSizeRule::kPreferredSnapToZero,
                    views::MaximumFlexSizeRule::kPreferred);
                toolbar_element->SetProperty(views::kFlexBehaviorKey,
                                             flex_spec);
              }
              flex_spec =
                  toolbar_element->GetProperty(views::kFlexBehaviorKey)
                      ->WithOrder(id_to_order_map.at(id.overflow_identifier));
              toolbar_element->SetProperty(views::kFlexBehaviorKey, flex_spec);

              // Create pop out state and pop out handlers to support pop out.
              if (id.observed_identifier.has_value()) {
                auto state = std::make_unique<PopOutState>();
                if (original_spec) {
                  state->original_spec =
                      std::optional<views::FlexSpecification>(*original_spec);
                }
                state->responsive_spec = flex_spec;
                state->handler = std::make_unique<PopOutHandler>(
                    this,
                    views::ElementTrackerViews::GetContextForView(
                        toolbar_container_view),
                    id.overflow_identifier, id.observed_identifier.value());
                pop_out_state_[id.overflow_identifier] = std::move(state);
              }
            }},
        overflow_id);
  }

  const auto it = id_to_order_map.find(kWebUIToolbarElementIdentifier);
  // There may be no `kWebUIToolbarElementIdentifier` entry in unit tests.
  if (it != id_to_order_map.end()) {
    webui_toolbar_button_flex_order_ = it->second;
  }
}

ToolbarController::~ToolbarController() = default;

std::vector<ui::ElementIdentifier>
ToolbarController::GetDefaultOverflowOrder() {
  std::vector<ui::ElementIdentifier> order = {
      kToolbarMediaButtonElementId, kToolbarBatterySaverButtonElementId,
      kToolbarHomeButtonElementId,
      // `kWebUIToolbarElementIdentifier` is a placeholder element representing
      // the order it uses for both the home and forward buttons, if it's
      // displaying them. Using a value in the middle of the two means that it
      // uses the correct relative order, even when only one of the two buttons
      // is being handled by the WebUI toolbar.
      kWebUIToolbarElementIdentifier, kToolbarForwardButtonElementId,
      kToolbarAvatarButtonElementId, kToolbarSplitTabsToolbarButtonElementId,
      kPinnedToolbarActionShowSidePanelContextualTasksElementId};
  if (base::FeatureList::IsEnabled(features::kToolbarGlicButtonResizing)) {
    const auto it =
        std::find(order.begin(), order.end(), kToolbarAvatarButtonElementId);
    order.insert(it, kGlicButtonElementId);
  }
  return order;
}

// Every activate identifier should have an action name in order to emit
// metrics. Please update action names in actions.xml to match this map.
std::string ToolbarController::GetActionNameFromElementIdentifier(
    std::variant<ui::ElementIdentifier, actions::ActionId> identifier) {
  static const base::NoDestructor<base::flat_map<
      std::variant<ui::ElementIdentifier, actions::ActionId>, std::string_view>>
      identifier_to_action_name_map(
          {{kToolbarAvatarButtonElementId, "AvatarButton"},
           {kToolbarBatterySaverButtonElementId, "BatterySaverButton"},
           {kExtensionsMenuButtonElementId, "ExtensionsMenuButton"},
           {kToolbarForwardButtonElementId, "ForwardButton"},
           {kActionForward, "ForwardButton"},
           {kToolbarHomeButtonElementId, "HomeButton"},
           {kActionHome, "HomeButton"},
           {kToolbarMediaButtonElementId, "MediaButton"},
           {kToolbarSidePanelButtonElementId, "SidePanelButton"},
           {kToolbarSplitTabsToolbarButtonElementId, "SplitTabs"},
           {kPinnedToolbarActionShowSidePanelContextualTasksElementId,
            "PinnedContextualTasksSidePanelButton"},
           {kActionSidePanelShowContextualTasks,
            "PinnedContextualTasksSidePanelButton"},
           {kActionClearBrowsingData, "PinnedClearBrowsingDataButton"},
           {kActionCopyUrl, "PinnedCopyLinkButton"},
           {kActionDevTools, "PinnedDeveloperToolsButton"},
           {kActionNewIncognitoWindow, "PinnedNewIncognitoWindowButton"},
           {kActionPrint, "PinnedPrintButton"},
           {kActionQrCodeGenerator, "PinnedQrCodeGeneratorButton"},
           {kActionRouteMedia, "PinnedCastButton"},
           {kActionSendTabToSelf, "PinnedSendTabToSelfButton"},
           {kActionShowAddresses, "PinnedShowAddressesBubbleOrPageButton"},
           {kActionShowAddressesBubbleOrPage,
            "PinnedShowAddressesBubbleOrPageButton"},
           {kActionShowChromeLabs, "PinnedShowChromeLabsButton"},
           {kActionShowDownloads, "PinnedShowDownloadsButton"},
           {kActionShowPasswordManager,
            "PinnedShowPasswordsBubbleOrPageButton"},
           {kActionShowPasswordsBubbleOrPage,
            "PinnedShowPasswordsBubbleOrPageButton"},
           {kActionShowPaymentMethods, "PinnedShowPaymentsBubbleOrPageButton"},
           {kActionShowPaymentsBubbleOrPage,
            "PinnedShowPaymentsBubbleOrPageButton"},
           {kActionShowTranslate, "PinnedShowTranslateButton"},
           {kActionSidePanelShowBookmarks, "PinnedShowBookmarkSidePanelButton"},
           {kActionSidePanelShowReadAnything,
            "PinnedShowReadAnythingSidePanelButton"},
           {kActionSidePanelShowHistoryCluster,
            "PinnedShowHistorySidePanelButton"},
           {kActionSidePanelShowReadingList,
            "PinnedShowReadingListSidePanelButton"},
           {kActionSidePanelShowSearchCompanion,
            "PinnedShowSearchCompanionSidePanelButton"},
           {kActionTaskManager, "PinnedTaskManagerButton"},
           {kActionSidePanelShowLensOverlayResults,
            "PinnedShowLensOverlayResultsSidePanelButton"},
           {kActionSendSharedTabGroupFeedback, "SharedTabGroupFeedbackButton"},
           {kActionTabSearch, "PinnedTabSearchButton"},
           {kActionSidePanelShowGlic, "PinnedGlicButton"},
           {kActionSidePanelShowTabsFromOtherDevices,
            "PinnedTabsFromOtherDevicesButton"},
           {kGlicButtonElementId, "GlicButtonElementId"}});

  const auto it = identifier_to_action_name_map->find(identifier);
  return it == identifier_to_action_name_map->end()
             ? std::string()
             : base::StrCat({"ResponsiveToolbar.OverflowMenuItemActivated.",
                             it->second});
}

bool ToolbarController::PopOut(ui::ElementIdentifier identifier,
                               bool show_synchronously) {
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
    if (base::FeatureList::IsEnabled(features::kToolbarProfileChipResizing)) {
      // Some elements (e.g. profile chip) use flex rules that allow
      // snapping/scaling to zero. When popping out, elements should never be
      // below the mininmum size.
      element->SetProperty(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                                   views::MinimumFlexSizeRule::kScaleToMinimum,
                                   views::MaximumFlexSizeRule::kPreferred)
              .WithOrder((*original).order())
              .WithWeight((*original).weight())
              .WithAlignment((*original).alignment()));
    } else {
      element->SetProperty(views::kFlexBehaviorKey, original.value());
    }
  } else {
    element->ClearProperty(views::kFlexBehaviorKey);
  }

  element->parent()->InvalidateLayout();
  if (show_synchronously) {
    toolbar_container_view_->DeprecatedLayoutImmediately();
  }
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

bool ToolbarController::ShouldShowOverflowButton(gfx::Size available_size) {
  if (ToolbarControllerUtil::PreventOverflow()) {
    return false;
  }

  // Once at least one button has been dropped by layout manager show overflow
  // button. Be sure to exclude the overflow button from the calculation.
  views::ManualLayoutUtil manual_layout_util(
      static_cast<views::LayoutManagerBase*>(
          toolbar_container_view_->GetLayoutManager()));
  const auto exclusion =
      manual_layout_util.TemporarilyExcludeFromLayout(overflow_button());
  views::ProposedLayout proposed_layout =
      static_cast<views::LayoutManagerBase*>(
          toolbar_container_view_->GetLayoutManager())
          ->GetProposedLayout(available_size);

  // Check if any buttons should overflow from pinned action delegate given the
  // available size.
  if (pinned_actions_delegate_) {
    if (views::ChildLayout* child_layout = proposed_layout.GetLayoutFor(
            pinned_actions_delegate_->GetContainerView())) {
      if (pinned_actions_delegate_->ShouldAnyButtonsOverflow(gfx::Size(
              child_layout->bounds.width(), child_layout->bounds.height()))) {
        return true;
      }
    }
  }

  for (const auto& element : overflow_menu_.responsive_elements()) {
    // Skip if it's an ActionId because it's already checked.
    if (std::holds_alternative<actions::ActionId>(element.overflow_id)) {
      continue;
    }
    if (IsOverflowed(element.overflow_id, &proposed_layout)) {
      return true;
    }
  }
  return false;
}

bool ToolbarController::InOverflowMode() const {
  return overflow_button_->GetVisible();
}

bool ToolbarController::IsElementOverflowedForTesting(
    ui::ElementIdentifier id) const {
  for (const auto& responsive_element : overflow_menu_.responsive_elements()) {
    const auto* element_id_info = std::get_if<ToolbarController::ElementIdInfo>(
        &responsive_element.overflow_id);
    if (!element_id_info || element_id_info->overflow_identifier != id) {
      continue;
    }
    return IsOverflowed(responsive_element.overflow_id);
  }
  // Element cannot overflow, since it is not in `responsive_elements_`
  NOTREACHED();
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
  for (views::View* child : view->children()) {
    if (auto* result = FindToolbarElementWithId(child, id)) {
      return result;
    }
  }
  return nullptr;
}

std::vector<const ToolbarController::ResponsiveElementInfo*>
ToolbarController::GetOverflowedElements() const {
  std::vector<const ToolbarController::ResponsiveElementInfo*>
      overflowed_buttons;
  if (ToolbarControllerUtil::PreventOverflow()) {
    return overflowed_buttons;
  }
  for (const auto& element : overflow_menu_.responsive_elements()) {
    if (IsOverflowed(element.overflow_id)) {
      overflowed_buttons.push_back(&element);
    }
  }
  return overflowed_buttons;
}

bool ToolbarController::IsOverflowed(
    const OverflowableElement& element,
    const views::ProposedLayout* proposed_layout) const {
  return std::visit(
      absl::Overload{
          [&](actions::ActionId id) {
            CHECK(!proposed_layout);
            return pinned_actions_delegate_ &&
                   pinned_actions_delegate_->IsOverflowed(id);
          },
          [&](const ToolbarController::ElementIdInfo& id) {
            const auto* const toolbar_element = FindToolbarElementWithId(
                toolbar_container_view_, id.overflow_identifier);
            // If the element is on the toolbar, it's being handled by Views, so
            // check the state of the Views element.
            if (toolbar_element) {
              const views::FlexLayout* const flex_layout =
                  static_cast<views::FlexLayout*>(
                      toolbar_container_view_->GetLayoutManager());
              return flex_layout->CanBeVisible(toolbar_element) &&
                     !(proposed_layout
                           ? proposed_layout->GetLayoutFor(toolbar_element)
                                 ->visible
                           : toolbar_element->GetVisible());
            }
            // If the element is not on the toolbar, either it's being handled
            // by WebUI, or the element was not added to the toolbar. We do not
            // know which, so need to check with the WebUI toolbar in either
            // case, if there is a WebUI toolbar.
            if (webui_toolbar_controller_delegate_) {
              return webui_toolbar_controller_delegate_->IsOverflowed(
                  id.overflow_identifier, proposed_layout);
            } else {
              return false;
            }
          }},
      element);
}

void ToolbarController::ExecuteCommand(const OverflowableElement& element) {
  std::variant<ui::ElementIdentifier, actions::ActionId> action_key;
  std::visit(
      absl::Overload{
          [&, this](actions::ActionId id) {
            pinned_actions_delegate_->GetActionItemFor(id)->InvokeAction(
                actions::ActionInvocationContext::Builder()
                    .SetProperty(kSidePanelOpenTriggerKey,
                                 SidePanelOpenTrigger::kOverflowMenu)
                    .Build());
            action_key.emplace<actions::ActionId>(id);
          },
          [&, this](const ToolbarController::ElementIdInfo& id_info) {
            ui::ElementIdentifier activate_identifier =
                id_info.activate_identifier;
            const auto* const element_view = FindToolbarElementWithId(
                toolbar_container_view_, activate_identifier);
            if (element_view) {
              const auto* button = AsViewClass<views::Button>(element_view);
              button->button_controller()->NotifyClick();
            } else {
              // If an element is on the overflow menu, but has no toolbar
              // element, there must be a WebUI toolbar handling that element.
              CHECK(webui_toolbar_controller_delegate_);
              webui_toolbar_controller_delegate_->OverflowButtonClicked(
                  activate_identifier);
            }
            action_key.emplace<ui::ElementIdentifier>(activate_identifier);
          }},
      element);
  std::string action_name = GetActionNameFromElementIdentifier(action_key);
  if (!action_name.empty()) {
    base::RecordAction(
        base::UserMetricsAction("ResponsiveToolbar.OverflowMenuItemActivated"));
    base::RecordAction(base::UserMetricsAction(action_name.c_str()));
  }
}

bool ToolbarController::IsCurrentlyOverflowed(
    const OverflowableElement& element) const {
  return IsOverflowed(element);
}

bool ToolbarController::IsEnabled(const OverflowableElement& element) const {
  return std::visit(
      absl::Overload{
          [this](actions::ActionId id) {
            return pinned_actions_delegate_->GetActionItemFor(id)->GetEnabled();
          },
          [this](const ToolbarController::ElementIdInfo& id) {
            const views::View* view = FindToolbarElementWithId(
                toolbar_container_view_, id.overflow_identifier);
            if (view) {
              return view->GetEnabled();
            }
            // If an element is on the overflow menu, but has no toolbar
            // element, there must be a WebUI toolbar handling that element.
            CHECK(webui_toolbar_controller_delegate_);
            return webui_toolbar_controller_delegate_->IsEnabled(
                id.overflow_identifier);
          }},
      element);
}

void ToolbarController::OnMenuClosed() {}

void ToolbarController::ShowMenu() {
  CHECK(overflow_button_->GetVisible());
  auto* button_controller = overflow_button_->menu_button_controller();
  overflow_menu_.ShowMenu(
      button_controller->button()->GetWidget(), button_controller,
      button_controller->button()->GetAnchorBoundsInScreen());
}
