// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_controller.h"

#include <optional>
#include <string_view>

#include "base/functional/overloaded.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/toolbar_controller_util.h"
#include "chrome/browser/ui/views/side_panel/side_panel_action_callback.h"
#include "chrome/browser/ui/views/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/views/toolbar/overflow_button.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_button_status_indicator.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace {

// Status indicator of a menu item.
constexpr gfx::Rect kStatusRect(10, 2);
// Padding between the image container and the status indicator.
constexpr int kImageContainerLowerPadding = 1;

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

ToolbarController::ResponsiveElementInfo::ResponsiveElementInfo(
    absl::variant<ElementIdInfo, actions::ActionId> overflow_id,
    bool is_section_end,
    std::optional<ui::ElementIdentifier> observed_identifier)
    : overflow_id(overflow_id),
      is_section_end(is_section_end),
      observed_identifier(observed_identifier) {}
ToolbarController::ResponsiveElementInfo::ResponsiveElementInfo(
    const ResponsiveElementInfo& info) = default;
ToolbarController::ResponsiveElementInfo::~ResponsiveElementInfo() = default;

ToolbarController::ToolbarController(
    const std::vector<ToolbarController::ResponsiveElementInfo>&
        responsive_elements,
    const std::vector<ui::ElementIdentifier>& elements_in_overflow_order,
    int element_flex_order_start,
    views::View* toolbar_container_view,
    OverflowButton* overflow_button,
    ToolbarController::PinnedActionsDelegate* pinned_actions_delegate)
    : responsive_elements_(responsive_elements),
      element_flex_order_start_(element_flex_order_start),
      toolbar_container_view_(toolbar_container_view),
      overflow_button_(overflow_button),
      pinned_actions_delegate_(pinned_actions_delegate) {
  if (ToolbarControllerUtil::PreventOverflow()) {
    return;
  }

  for (auto& responsive_element : responsive_elements_) {
    if (absl::holds_alternative<actions::ActionId>(
            responsive_element.overflow_id)) {
      actions::ActionId action_id =
          absl::get<actions::ActionId>(responsive_element.overflow_id);
      actions::ActionItem* action_item =
          pinned_actions_delegate_->GetActionItemFor(action_id);

      action_changed_subscription_.push_back(
          action_item->AddActionChangedCallback(
              base::BindRepeating(&ToolbarController::ActionItemChanged,
                                  base::Unretained(this), action_item)));
    }
  }

  const auto id_to_order_map =
      CalculateFlexOrder(elements_in_overflow_order, element_flex_order_start);
  for (const auto& element : responsive_elements) {
    const auto& overflow_id = element.overflow_id;
    const auto& observed_identifier = element.observed_identifier;

    absl::visit(
        base::Overloaded{
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
              if (observed_identifier.has_value()) {
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
                    id.overflow_identifier, observed_identifier.value());
                pop_out_state_[id.overflow_identifier] = std::move(state);
              }
            }},
        overflow_id);
  }
}

ToolbarController::~ToolbarController() {
  CloseMenu();
}

std::vector<ToolbarController::ResponsiveElementInfo>
ToolbarController::GetDefaultResponsiveElements(Browser* browser) {
  bool is_incognito = browser->profile()->IsIncognitoProfile();
  // TODO(crbug.com/40912482): Fill in observed identifier.
  // Order matters because it should match overflow menu order top to bottom.
  std::vector<ToolbarController::ResponsiveElementInfo> elements = {
      {ToolbarController::ElementIdInfo{
           kToolbarForwardButtonElementId, IDS_OVERFLOW_MENU_ITEM_TEXT_FORWARD,
           &vector_icons::kForwardArrowChromeRefreshIcon,
           kToolbarForwardButtonElementId},
       /*is_section_end=*/false},
      {ToolbarController::ElementIdInfo{
           kToolbarHomeButtonElementId, IDS_OVERFLOW_MENU_ITEM_TEXT_HOME,
           &kNavigateHomeChromeRefreshIcon, kToolbarHomeButtonElementId},
       /*is_section_end=*/true}};

  // Support actions items.
  const auto* const browser_actions = browser->browser_actions();
  if (browser_actions) {
    auto* root_item = browser_actions->root_action_item();
    if (root_item) {
      for (const auto& item : root_item->GetChildren().children()) {
        auto id = item->GetActionId();
        if (item->GetProperty(actions::kActionItemPinnableKey) &&
            id.has_value()) {
          elements.emplace_back(id.value());
        }
      }
      auto& last_element = elements.back();
      if (absl::holds_alternative<actions::ActionId>(
              last_element.overflow_id)) {
        last_element.is_section_end = true;
      }
    }
  }

  elements.insert(
      elements.end(),
      {{ToolbarController::ElementIdInfo{
            kToolbarChromeLabsButtonElementId, IDS_OVERFLOW_MENU_ITEM_TEXT_LABS,
            &kScienceIcon, kToolbarChromeLabsButtonElementId},
        /*is_section_end=*/false, kToolbarChromeLabsBubbleElementId},
       {ToolbarController::ElementIdInfo{
            kToolbarMediaButtonElementId,
            IDS_OVERFLOW_MENU_ITEM_TEXT_MEDIA_CONTROLS,
            &kMediaToolbarButtonChromeRefreshIcon,
            kToolbarMediaButtonElementId},
        /*is_section_end=*/false, kToolbarMediaBubbleElementId},
       {ToolbarController::ElementIdInfo{
            kToolbarDownloadButtonElementId,
            IDS_OVERFLOW_MENU_ITEM_TEXT_DOWNLOADS,
            &kDownloadToolbarButtonChromeRefreshIcon,
            kToolbarDownloadButtonElementId},
        /*is_section_end=*/true, kToolbarDownloadBubbleElementId},
       {ToolbarController::ElementIdInfo{kToolbarNewTabButtonElementId,
                                         IDS_OVERFLOW_MENU_ITEM_TEXT_NEW_TAB,
#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
                                         &kNewTabToolbarButtonIcon,
#else
                                         nullptr,
#endif  // BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
                                         kToolbarNewTabButtonElementId},
        /*is_section_end=*/true},
       {ToolbarController::ElementIdInfo{
            kToolbarAvatarButtonElementId, IDS_OVERFLOW_MENU_ITEM_TEXT_PROFILE,
            is_incognito ? (&kIncognitoRefreshMenuIcon)
                         : (&kUserAccountAvatarRefreshIcon),
            kToolbarAvatarButtonElementId},
        /*is_section_end=*/false, kToolbarAvatarBubbleElementId}});
  return elements;
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
    absl::variant<ui::ElementIdentifier, actions::ActionId> identifier) {
  static const base::NoDestructor<
      base::flat_map<absl::variant<ui::ElementIdentifier, actions::ActionId>,
                     std::string_view>>
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
          {kActionClearBrowsingData, "PinnedClearBrowsingDataButton"},
          {kActionCopyUrl, "PinnedCopyLinkButton"},
          {kActionDevTools, "PinnedDeveloperToolsButton"},
          {kActionNewIncognitoWindow, "PinnedNewIncognitoWindowButton"},
          {kActionPrint, "PinnedPrintButton"},
          {kActionQrCodeGenerator, "PinnedQrCodeGeneratorButton"},
          {kActionRouteMedia, "PinnedCastButton"},
          {kActionSendTabToSelf, "PinnedSendTabToSelfButton"},
          {kActionShowAddressesBubbleOrPage,
           "PinnedShowAddressesBubbleOrPageButton"},
          {kActionShowChromeLabs, "PinnedShowChromeLabsButton"},
          {kActionShowPasswordsBubbleOrPage,
           "PinnedShowPasswordsBubbleOrPageButton"},
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

  for (const auto& element : responsive_elements_) {
    // Skip if it's an ActionId because it's already checked.
    if (absl::holds_alternative<actions::ActionId>(element.overflow_id)) {
      continue;
    }
    if (IsOverflowed(element, &proposed_layout)) {
      return true;
    }
  }
  return false;
}

bool ToolbarController::InOverflowMode() const {
  return overflow_button_->GetVisible();
}

std::u16string ToolbarController::GetMenuText(
    const ResponsiveElementInfo& element_info) const {
  return absl::visit(
      base::Overloaded{
          [this](actions::ActionId id) {
            return pinned_actions_delegate_->GetActionItemFor(id)->GetText();
          },
          [](ToolbarController::ElementIdInfo id) {
            return l10n_util::GetStringUTF16(id.menu_text_id);
          }},
      element_info.overflow_id);
}

std::optional<ui::ImageModel> ToolbarController::GetMenuIcon(
    const ResponsiveElementInfo& element_info) const {
  return absl::visit(
      base::Overloaded{
          [this](actions::ActionId id) {
            // Resize the vector icon to `kDefaultIconSize`.
            const ui::ImageModel& pinned_icon_image =
                pinned_actions_delegate_->GetActionItemFor(id)->GetImage();
            if (!pinned_icon_image.IsEmpty() &&
                pinned_icon_image.IsVectorIcon()) {
              ui::VectorIconModel vector_icon_model =
                  pinned_icon_image.GetVectorIcon();

              return std::make_optional(ui::ImageModel::FromVectorIcon(
                  *vector_icon_model.vector_icon(),
                  vector_icon_model.color_id(),
                  ui::SimpleMenuModel::kDefaultIconSize));
            } else {
              return std::make_optional(pinned_icon_image);
            }
          },
          [&](ToolbarController::ElementIdInfo info)
              -> std::optional<ui::ImageModel> {
            if (!info.menu_icon) {
              return std::nullopt;
            }
            return std::make_optional(ui::ImageModel::FromVectorIcon(
                *info.menu_icon, ui::kColorMenuIcon,
                ui::SimpleMenuModel::kDefaultIconSize));
          }},
      element_info.overflow_id);
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
ToolbarController::GetOverflowedElements() {
  std::vector<const ToolbarController::ResponsiveElementInfo*>
      overflowed_buttons;
  if (ToolbarControllerUtil::PreventOverflow()) {
    return overflowed_buttons;
  }
  for (const auto& element : responsive_elements_) {
    if (IsOverflowed(element)) {
      overflowed_buttons.push_back(&element);
    }
  }
  return overflowed_buttons;
}

bool ToolbarController::IsOverflowed(
    const ResponsiveElementInfo& element,
    const views::ProposedLayout* proposed_layout) const {
  return absl::visit(
      base::Overloaded{[&](actions::ActionId id) {
                         CHECK(!proposed_layout);
                         return pinned_actions_delegate_ &&
                                pinned_actions_delegate_->IsOverflowed(id);
                       },
                       [&](ToolbarController::ElementIdInfo id) {
                         const auto* const toolbar_element =
                             FindToolbarElementWithId(toolbar_container_view_,
                                                      id.overflow_identifier);
                         const views::FlexLayout* const flex_layout =
                             static_cast<views::FlexLayout*>(
                                 toolbar_container_view_->GetLayoutManager());
                         return flex_layout->CanBeVisible(toolbar_element) &&
                                !(proposed_layout
                                      ? proposed_layout
                                            ->GetLayoutFor(toolbar_element)
                                            ->visible
                                      : toolbar_element->GetVisible());
                       }},
      element.overflow_id);
}

std::unique_ptr<ui::SimpleMenuModel>
ToolbarController::CreateOverflowMenuModel() {
  CHECK(overflow_button_->GetVisible());
  auto menu_model = std::make_unique<ui::SimpleMenuModel>(this);

  // True if the separator belonging to previous section has not been added yet.
  bool pre_separator_pending = false;
  for (size_t i = 0; i < responsive_elements_.size(); ++i) {
    const auto& element = responsive_elements_[i];
    if (IsOverflowed(element)) {
      if (pre_separator_pending && menu_model->GetItemCount() > 0) {
        menu_model->AddSeparator(ui::NORMAL_SEPARATOR);
      }
      const auto image_model = GetMenuIcon(element);
      if (image_model.has_value()) {
        menu_model->AddItemWithIcon(i, GetMenuText(element),
                                    image_model.value());
      } else {
        menu_model->AddItem(i, GetMenuText(element));
      }
      pre_separator_pending = false;
    }
    if (element.is_section_end) {
      pre_separator_pending = true;
    }
  }
  return menu_model;
}

bool ToolbarController::IsCommandIdEnabled(int command_id) const {
  return absl::visit(
      base::Overloaded{
          [this](actions::ActionId id) {
            return pinned_actions_delegate_->GetActionItemFor(id)->GetEnabled();
          },
          [this](ToolbarController::ElementIdInfo id) {
            return FindToolbarElementWithId(toolbar_container_view_,
                                            id.overflow_identifier)
                ->GetEnabled();
          }},
      responsive_elements_.at(command_id).overflow_id);
}

void ToolbarController::ExecuteCommand(int command_id, int event_flags) {
  const auto& element_info = responsive_elements_.at(command_id);
  absl::variant<ui::ElementIdentifier, actions::ActionId> action_key;
  absl::visit(
      base::Overloaded{
          [&, this](actions::ActionId id) {
            pinned_actions_delegate_->GetActionItemFor(id)->InvokeAction(
                actions::ActionInvocationContext::Builder()
                    .SetProperty(
                        kSidePanelOpenTriggerKey,
                        static_cast<
                            std::underlying_type_t<SidePanelOpenTrigger>>(
                            SidePanelOpenTrigger::kOverflowMenu))
                    .Build());
            action_key.emplace<actions::ActionId>(id);
          },
          [&, this](ToolbarController::ElementIdInfo id) {
            const auto& activate_identifier = id.activate_identifier;
            const auto* const element = FindToolbarElementWithId(
                toolbar_container_view_, activate_identifier);
            CHECK(element);
            const auto* button = AsViewClass<views::Button>(element);
            button->button_controller()->NotifyClick();
            action_key.emplace<ui::ElementIdentifier>(activate_identifier);
          }},
      element_info.overflow_id);
  std::string action_name = GetActionNameFromElementIdentifier(action_key);
  if (!action_name.empty()) {
    base::RecordAction(
        base::UserMetricsAction("ResponsiveToolbar.OverflowMenuItemActivated"));
    base::RecordAction(base::UserMetricsAction(action_name.c_str()));
  }
}

void ToolbarController::ShowStatusIndicator() {
  views::SubmenuView* sub_menu = root_menu_item_->GetSubmenu();

  // Install the status indicator and show it if it is active.
  for (auto* menu_item : sub_menu->GetMenuItems()) {
    if (!menu_item->icon_view()) {
      continue;
    }

    // Layout of the status indicator.
    PinnedToolbarButtonStatusIndicator* status_indicator =
        PinnedToolbarButtonStatusIndicator::Install(menu_item->icon_view());
    status_indicator->SetColorId(kColorToolbarActionItemEngaged,
                                 kColorToolbarButtonIconInactive);

    gfx::Rect status_rect = kStatusRect;
    const gfx::Rect image_container_bounds =
        menu_item->icon_view()->GetLocalBounds();

    const int new_x =
        image_container_bounds.x() +
        (image_container_bounds.width() - status_rect.width()) / 2;
    const int new_y =
        image_container_bounds.bottom() + kImageContainerLowerPadding;

    // Set the new origin for status_rect
    status_rect.set_origin(gfx::Point(new_x, new_y));
    status_indicator->SetBoundsRect(status_rect);

    if (absl::holds_alternative<actions::ActionId>(
            responsive_elements_.at(menu_item->GetCommand()).overflow_id)) {
      actions::ActionId action_id = absl::get<actions::ActionId>(
          responsive_elements_.at(menu_item->GetCommand()).overflow_id);
      actions::ActionItem* action_item =
          pinned_actions_delegate_->GetActionItemFor(action_id);

      if (action_item &&
          action_item->GetProperty(kActionItemUnderlineIndicatorKey)) {
        const ui::ImageModel& pinned_icon_image = action_item->GetImage();
        if (!pinned_icon_image.IsEmpty() && pinned_icon_image.IsVectorIcon()) {
          ui::VectorIconModel vector_icon_model =
              pinned_icon_image.GetVectorIcon();

          menu_item->icon_view()->SetImage(gfx::CreateVectorIcon(
              *vector_icon_model.vector_icon(),
              ui::SimpleMenuModel::kDefaultIconSize,
              menu_item->icon_view()->GetColorProvider()->GetColor(
                  kColorToolbarActionItemEngaged)));
        }
        status_indicator->Show();
      }
    }
  }
}

void ToolbarController::ActionItemChanged(actions::ActionItem* action_item) {
  if (!IsMenuRunning()) {
    return;
  }

  std::optional<int> command_id = std::nullopt;
  for (size_t i = 0; i < responsive_elements_.size(); ++i) {
    const auto& element = responsive_elements_[i];
    if (absl::holds_alternative<actions::ActionId>(element.overflow_id)) {
      actions::ActionId element_action_id =
          absl::get<actions::ActionId>(element.overflow_id);
      if (element_action_id == action_item->GetActionId().value()) {
        command_id = static_cast<int>(i);
        break;
      }
    }
  }

  if (!IsOverflowed(responsive_elements_.at(command_id.value()))) {
    return;
  }

  views::MenuItemView* menu_item =
      root_menu_item_->GetMenuItemByID(command_id.value());

  if (!menu_item || !menu_item->icon_view()) {
    return;
  }

  PinnedToolbarButtonStatusIndicator* status_indicator =
      PinnedToolbarButtonStatusIndicator::GetStatusIndicator(
          menu_item->icon_view());

  if (!status_indicator) {
    return;
  }

  if (action_item->GetProperty(kActionItemUnderlineIndicatorKey)) {
    const ui::ImageModel& pinned_icon_image = action_item->GetImage();
    if (!pinned_icon_image.IsEmpty() && pinned_icon_image.IsVectorIcon()) {
      ui::VectorIconModel vector_icon_model = pinned_icon_image.GetVectorIcon();

      menu_item->icon_view()->SetImage(gfx::CreateVectorIcon(
          *vector_icon_model.vector_icon(),
          ui::SimpleMenuModel::kDefaultIconSize,
          menu_item->icon_view()->GetColorProvider()->GetColor(
              kColorToolbarActionItemEngaged)));
    }
    status_indicator->Show();
  } else {
    const ui::ImageModel& pinned_icon_image = action_item->GetImage();
    if (!pinned_icon_image.IsEmpty() && pinned_icon_image.IsVectorIcon()) {
      ui::VectorIconModel vector_icon_model = pinned_icon_image.GetVectorIcon();

      menu_item->icon_view()->SetImage(gfx::CreateVectorIcon(
          *vector_icon_model.vector_icon(),
          ui::SimpleMenuModel::kDefaultIconSize,
          menu_item->icon_view()->GetColorProvider()->GetColor(
              vector_icon_model.color_id())));
    }
    status_indicator->Hide();
  }
}

void ToolbarController::PopulateMenu(views::MenuItemView* parent) {
  if (parent->HasSubmenu()) {
    parent->GetSubmenu()->RemoveAllChildViews();
  }

  if (menu_model_) {
    menu_model_->Clear();
  }

  menu_model_ = CreateOverflowMenuModel();
  CHECK(menu_model_);

  for (size_t i = 0; i < menu_model_->GetItemCount(); ++i) {
    views::MenuItemView* menu_item =
        views::MenuModelAdapter::AppendMenuItemFromModel(
            menu_model_.get(), i, parent, menu_model_->GetCommandIdAt(i));

    // `menu_item` can be nullptr if it is a separator.
    if (menu_item &&
        menu_item->GetType() == views::MenuItemView::Type::kNormal) {
      menu_item->SetEnabled(IsCommandIdEnabled(menu_item->GetCommand()));
    }
  }

  parent->GetSubmenu()->InvalidateLayout();
}

void ToolbarController::ShowMenu() {
  auto* button_controller = overflow_button_->menu_button_controller();
  auto root = std::make_unique<views::MenuItemView>(this);
  root_menu_item_ = root.get();
  PopulateMenu(root_menu_item_);

  menu_runner_ = std::make_unique<views::MenuRunner>(
      std::move(root), views::MenuRunner::HAS_MNEMONICS);
  menu_runner_->RunMenuAt(
      button_controller->button()->GetWidget(), button_controller,
      button_controller->button()->GetAnchorBoundsInScreen(),
      views::MenuAnchorPosition::kTopRight, ui::MENU_SOURCE_NONE);
  ShowStatusIndicator();
}

bool ToolbarController::IsMenuRunning() const {
  return menu_runner_ && menu_runner_->IsRunning();
}

void ToolbarController::CloseMenu() {
  root_menu_item_ = nullptr;
  menu_model_.reset();
  menu_runner_.reset();
}
