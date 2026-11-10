// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_controller.h"

#include <algorithm>
#include <array>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "base/containers/fixed_flat_map.h"
#include "base/containers/flat_map.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
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
#include "ui/views/widget/widget.h"

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

              // Copy any existing spec by value before calling `SetProperty()`,
              // which mutates an existing `FlexSpecification` allocation in
              // place.
              std::optional<views::FlexSpecification> original_spec;
              if (const views::FlexSpecification* existing_spec =
                      toolbar_element->GetProperty(views::kFlexBehaviorKey)) {
                original_spec = *existing_spec;
              }
              views::FlexSpecification flex_spec =
                  original_spec.value_or(views::FlexSpecification(
                      views::LayoutOrientation::kHorizontal,
                      views::MinimumFlexSizeRule::kPreferredSnapToZero,
                      views::MaximumFlexSizeRule::kPreferred));
              flex_spec = flex_spec.WithOrder(
                  id_to_order_map.at(id.overflow_identifier));
              toolbar_element->SetProperty(views::kFlexBehaviorKey, flex_spec);

              // Create pop out state for every responsive element, and attach a
              // PopOutHandler when an observed element identifier is
              // configured.
              auto state = std::make_unique<PopOutState>();
              state->original_spec = std::move(original_spec);
              state->responsive_spec = flex_spec;
              if (id.observed_identifier.has_value()) {
                state->handler = std::make_unique<PopOutHandler>(
                    this,
                    views::ElementTrackerViews::GetContextForView(
                        toolbar_container_view),
                    id.overflow_identifier, id.observed_identifier.value());
              }
              pop_out_state_[id.overflow_identifier] = std::move(state);
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
// metrics. Please update action names in actions.xml to match these tables.
std::string ToolbarController::GetActionNameFromElementIdentifier(
    std::variant<ui::ElementIdentifier, actions::ActionId> identifier) {
  static constexpr auto kElementIdToActionName =
      std::to_array<std::pair<ui::ElementIdentifier, std::string_view>>({
          {kToolbarAvatarButtonElementId, "AvatarButton"},
          {kToolbarBatterySaverButtonElementId, "BatterySaverButton"},
          {kExtensionsMenuButtonElementId, "ExtensionsMenuButton"},
          {kToolbarForwardButtonElementId, "ForwardButton"},
          {kToolbarHomeButtonElementId, "HomeButton"},
          {kToolbarMediaButtonElementId, "MediaButton"},
          {kToolbarSidePanelButtonElementId, "SidePanelButton"},
          {kToolbarSplitTabsToolbarButtonElementId, "SplitTabs"},
          {kPinnedToolbarActionShowSidePanelContextualTasksElementId,
           "PinnedContextualTasksSidePanelButton"},
          {kGlicButtonElementId, "GlicButtonElementId"},
      });

  static constexpr auto kActionIdToActionNameMap =
      base::MakeFixedFlatMap<actions::ActionId, std::string_view>({
          {kActionForward, "ForwardButton"},
          {kActionHome, "HomeButton"},
          {kActionSidePanelShowContextualTasks,
           "PinnedContextualTasksSidePanelButton"},
          {kActionClearBrowsingData, "PinnedClearBrowsingDataButton"},
          {kActionCopyUrl, "PinnedCopyLinkButton"},
          {kActionDevTools, "PinnedDeveloperToolsButton"},
          {kActionNewIncognitoWindow, "PinnedNewIncognitoWindowButton"},
          {kActionNewIsolatedWindow, "PinnedNewIsolatedWindowButton"},
          {kActionPrint, "PinnedPrintButton"},
          {kActionQrCodeGenerator, "PinnedQrCodeGeneratorButton"},
          {kActionRouteMedia, "PinnedCastButton"},
          {kActionSendTabToSelf, "PinnedSendTabToSelfButton"},
          {kActionShowAddressesBubbleOrPage,
           "PinnedShowAddressesBubbleOrPageButton"},
          {kActionShowChromeLabs, "PinnedShowChromeLabsButton"},
          {kActionShowDownloads, "PinnedShowDownloadsButton"},
          {kActionShowPasswordManager, "PinnedShowPasswordsBubbleOrPageButton"},
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
          {kActionTtcToolbar, "PinnedTtcButton"},
          {kActionSidePanelShowLensOverlayResults,
           "PinnedShowLensOverlayResultsSidePanelButton"},
          {kActionSendSharedTabGroupFeedback, "SharedTabGroupFeedbackButton"},
          {kActionTabSearch, "PinnedTabSearchButton"},
          {kActionSidePanelShowGlic, "PinnedGlicButton"},
          {kActionSidePanelShowTabsFromOtherDevices,
           "PinnedTabsFromOtherDevicesButton"},
      });

  const std::string_view suffix = std::visit(
      absl::Overload{
          [](ui::ElementIdentifier id) -> std::string_view {
            const auto it = std::ranges::find(
                kElementIdToActionName, id,
                &std::pair<ui::ElementIdentifier, std::string_view>::first);
            return it == kElementIdToActionName.end() ? std::string_view()
                                                      : it->second;
          },
          [](actions::ActionId id) -> std::string_view {
            const auto it = kActionIdToActionNameMap.find(id);
            return it == kActionIdToActionNameMap.end() ? std::string_view()
                                                        : it->second;
          }},
      identifier);

  return suffix.empty()
             ? std::string()
             : base::StrCat(
                   {"ResponsiveToolbar.OverflowMenuItemActivated.", suffix});
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

  const bool was_popped_out = it->second->scoped_pop_out_count > 0;
  it->second->is_popped_out = true;

  if (!was_popped_out) {
    ApplyPopOutSpec(element, *it->second);
  }
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
  MaybeRestoreResponsiveSpec(element, *it->second);
  return true;
}

base::ScopedClosureRunner ToolbarController::CreateScopedPopOut(
    ui::ElementIdentifier identifier) {
  if (ToolbarControllerUtil::PreventOverflow()) {
    return base::ScopedClosureRunner();
  }
  views::View* const element =
      FindToolbarElementWithId(toolbar_container_view_, identifier);
  if (!element) {
    return base::ScopedClosureRunner();
  }
  const auto it = pop_out_state_.find(identifier);
  if (it == pop_out_state_.end()) {
    return base::ScopedClosureRunner();
  }

  const bool was_popped_out = it->second->IsPoppedOut();
  ++it->second->scoped_pop_out_count;
  if (!was_popped_out) {
    ApplyPopOutSpec(element, *it->second);
  }
  if (views::Widget* widget = toolbar_container_view_->GetWidget()) {
    widget->LayoutRootViewIfNecessary();
  }

  return base::ScopedClosureRunner(
      base::BindOnce(&ToolbarController::ReleaseScopedPopOut,
                     weak_ptr_factory_.GetWeakPtr(), identifier));
}

void ToolbarController::ApplyPopOutSpec(views::View* element,
                                        const PopOutState& state) {
  if (state.original_spec.has_value()) {
    if (base::FeatureList::IsEnabled(features::kToolbarProfileChipResizing)) {
      // Some elements (e.g. profile chip) use flex rules that allow
      // snapping/scaling to zero. When popping out, elements should never be
      // below the minimum size.
      element->SetProperty(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                                   views::MinimumFlexSizeRule::kScaleToMinimum,
                                   views::MaximumFlexSizeRule::kPreferred)
              .WithOrder(state.original_spec->order())
              .WithWeight(state.original_spec->weight())
              .WithAlignment(state.original_spec->alignment()));
    } else {
      element->SetProperty(views::kFlexBehaviorKey, *state.original_spec);
    }
  } else {
    element->ClearProperty(views::kFlexBehaviorKey);
  }

  element->parent()->InvalidateLayout();
}

void ToolbarController::MaybeRestoreResponsiveSpec(views::View* element,
                                                   const PopOutState& state) {
  if (state.IsPoppedOut()) {
    return;
  }
  element->SetProperty(views::kFlexBehaviorKey, state.responsive_spec);
  element->parent()->InvalidateLayout();
}

void ToolbarController::ReleaseScopedPopOut(ui::ElementIdentifier identifier) {
  const auto it = pop_out_state_.find(identifier);
  CHECK(it != pop_out_state_.end());
  CHECK_GT(it->second->scoped_pop_out_count, 0);
  --it->second->scoped_pop_out_count;

  // `toolbar_container_view_` may have removed the child view before the scoped
  // token is released (e.g. unpinning a responsive button).
  if (views::View* const element =
          FindToolbarElementWithId(toolbar_container_view_, identifier)) {
    MaybeRestoreResponsiveSpec(element, *it->second);
  }
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
  const views::ProposedLayout& proposed_layout =
      static_cast<views::LayoutManagerBase*>(
          toolbar_container_view_->GetLayoutManager())
          ->GetProposedLayout(available_size);

  // Check if any buttons should overflow from pinned action delegate given the
  // available size.
  if (pinned_actions_delegate_) {
    if (const views::ChildLayout* child_layout = proposed_layout.GetLayoutFor(
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
