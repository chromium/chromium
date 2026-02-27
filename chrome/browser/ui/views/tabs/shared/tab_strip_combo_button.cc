// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/shared/tab_strip_combo_button.h"

#include "base/i18n/rtl.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_everything_menu.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tab_search_bubble_host.h"
#include "chrome/browser/ui/views/tabs/shared/tab_strip_flat_edge_button.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/saved_tab_groups/public/features.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/actions/action_view_controller.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"

namespace {
constexpr base::TimeDelta kHideTabSearchButtonDelay = base::Seconds(2);
constexpr base::TimeDelta kAnimationDuration = base::Milliseconds(300);
}  // namespace

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TabStripComboButton,
                                      kTabSearchUnpinMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TabStripComboButton,
                                      kProjectsPanelUnpinMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TabStripComboButton,
                                      kEverythingMenuUnpinMenuItem);

TabStripComboButton::TabStripComboButton(BrowserWindowInterface* browser)
    : browser_(browser),
      action_view_controller_(std::make_unique<views::ActionViewController>()) {
  start_button_animation_.SetSlideDuration(kAnimationDuration);
  end_button_animation_.SetSlideDuration(kAnimationDuration);
  SetProperty(views::kElementIdentifierKey, kTabStripComboButtonElementId);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      GetLayoutConstant(
          LayoutConstant::kVerticalTabStripFlatEdgeButtonPadding)));

  std::unique_ptr<TabStripFlatEdgeButton> start_button;
  if (tab_groups::IsProjectsPanelFeatureEnabled()) {
    start_button = CreateFlatEdgeButtonFor(
        kActionToggleProjectsPanel, kVerticalTabStripProjectsButtonElementId);
  } else if (tab_groups::SavedTabGroupUtils::IsEnabledForProfile(
                 browser_->GetProfile())) {
    start_button = CreateFlatEdgeButtonFor(kActionTabGroupsMenu,
                                           kSavedTabGroupButtonElementId);

    auto controller = std::make_unique<views::MenuButtonController>(
        start_button.get(),
        base::BindRepeating(&TabStripComboButton::ShowEverythingMenu,
                            base::Unretained(this)),
        std::make_unique<views::Button::DefaultButtonControllerDelegate>(
            start_button.get()));
    everything_menu_controller_ = controller.get();
    start_button->SetButtonController(std::move(controller));
  }

  if (start_button) {
    start_button_ = AddChildView(std::move(start_button));
  }
  end_button_ = AddChildView(
      CreateFlatEdgeButtonFor(kActionTabSearch, kTabSearchButtonElementId));

  PrefService* prefs = browser_->GetProfile()->GetPrefs();
  pref_registrar_.Init(prefs);
  pref_registrar_.Add(
      prefs::kTabSearchPinnedToTabstrip,
      base::BindRepeating(&TabStripComboButton::UpdateButtonsVisibility,
                          base::Unretained(this)));
  pref_registrar_.Add(
      prefs::kProjectsPanelPinnedToTabstrip,
      base::BindRepeating(&TabStripComboButton::UpdateButtonsVisibility,
                          base::Unretained(this)));
  pref_registrar_.Add(
      prefs::kEverythingMenuPinnedToTabstrip,
      base::BindRepeating(&TabStripComboButton::UpdateButtonsVisibility,
                          base::Unretained(this)));
  UpdateButtonsVisibility();
  UpdateStyles();
}

TabStripComboButton::~TabStripComboButton() {
  tab_search_bubble_host_observation_.Reset();
}

void TabStripComboButton::UpdateButtonsVisibility() {
  if (!browser_ || !browser_->GetActions()) {
    return;
  }
  auto update_button_visibility = [&](actions::ActionItem* action_item,
                                      gfx::SlideAnimation& animation,
                                      bool target_visible) {
    if (!action_item) {
      return;
    }

    if (target_visible) {
      action_item->SetVisible(true);
      animation.Show();
    } else {
      animation.Hide();
    }
    if (!animation.is_animating() && animation.GetCurrentValue() == 0.0) {
      action_item->SetVisible(false);
    }
  };

  PrefService* prefs = browser_->GetProfile()->GetPrefs();
  const std::string_view pref_name =
      tab_groups::IsProjectsPanelFeatureEnabled()
          ? prefs::kProjectsPanelPinnedToTabstrip
          : prefs::kEverythingMenuPinnedToTabstrip;
  update_button_visibility(GetStartButtonActionItem(), start_button_animation_,
                           prefs->GetBoolean(pref_name));

  update_button_visibility(
      GetEndButtonActionItem(), end_button_animation_,
      prefs->GetBoolean(prefs::kTabSearchPinnedToTabstrip) ||
          show_tab_search_ephemerally_);
}

void TabStripComboButton::SetOrientation(views::LayoutOrientation orientation) {
  if (orientation_ == orientation) {
    return;
  }
  orientation_ = orientation;

  views::BoxLayout* layout = static_cast<views::BoxLayout*>(GetLayoutManager());
  layout->SetOrientation(orientation_);
  layout->set_between_child_spacing(GetLayoutConstant(
      LayoutConstant::kVerticalTabStripFlatEdgeButtonPadding));

  UpdateStyles();
}

void TabStripComboButton::ChildVisibilityChanged(views::View* child) {
  UpdateStyles();
}

void TabStripComboButton::ShowEverythingMenu() {
  base::RecordAction(base::UserMetricsAction(
      BrowserView::GetBrowserViewForBrowser(browser_)
              ->ShouldDrawVerticalTabStrip()
          ? "TabGroups_SavedTabGroups_EverythingButtonPressed_Vertical"
          : "TabGroups_SavedTabGroups_EverythingButtonPressed_Horizontal"));
  if (everything_menu_ && everything_menu_->IsShowing()) {
    return;
  }

  everything_menu_ = std::make_unique<tab_groups::STGEverythingMenu>(
      everything_menu_controller_, browser_->GetBrowserForMigrationOnly(),
      tab_groups::STGEverythingMenu::MenuContext::kVerticalTabStrip);

  everything_menu_->RunMenu();
}

std::unique_ptr<TabStripFlatEdgeButton>
TabStripComboButton::CreateFlatEdgeButtonFor(actions::ActionId action_id,
                                             ui::ElementIdentifier element_id) {
  auto button = std::make_unique<TabStripFlatEdgeButton>();
  button->set_context_menu_controller(this);
  if (!browser_ || !browser_->GetActions()) {
    return button;
  }
  actions::ActionItem* action_item = actions::ActionManager::Get().FindAction(
      action_id, browser_->GetActions()->root_action_item());
  CHECK(action_item);
  action_view_controller_->CreateActionViewRelationship(
      button.get(), action_item->GetAsWeakPtr());
  button->SetProperty(views::kElementIdentifierKey, element_id);

  return button;
}

void TabStripComboButton::UpdateStyles() {
  const bool both_visible = start_button_ && start_button_->GetVisible() &&
                            end_button_ && end_button_->GetVisible();
  const bool is_vertical = orientation_ == views::LayoutOrientation::kVertical;

  if (start_button_) {
    TabStripFlatEdgeButton::FlatEdge flat_edge =
        TabStripFlatEdgeButton::FlatEdge::kNone;
    if (both_visible) {
      if (is_vertical) {
        flat_edge = TabStripFlatEdgeButton::FlatEdge::kBottom;
      } else {
        flat_edge = base::i18n::IsRTL()
                        ? TabStripFlatEdgeButton::FlatEdge::kLeft
                        : TabStripFlatEdgeButton::FlatEdge::kRight;
      }
    }
    start_button_->SetFlatEdge(flat_edge);
  }

  if (end_button_) {
    TabStripFlatEdgeButton::FlatEdge flat_edge =
        TabStripFlatEdgeButton::FlatEdge::kNone;
    if (both_visible) {
      if (is_vertical) {
        flat_edge = TabStripFlatEdgeButton::FlatEdge::kTop;
      } else {
        flat_edge = base::i18n::IsRTL()
                        ? TabStripFlatEdgeButton::FlatEdge::kRight
                        : TabStripFlatEdgeButton::FlatEdge::kLeft;
      }
    }
    end_button_->SetFlatEdge(flat_edge);
  }
}

void TabStripComboButton::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::mojom::MenuSourceType source_type) {
  int command_id = -1;
  int string_id = -1;
  ui::ElementIdentifier element_id;
  std::string_view pref_name;

  PrefService* prefs = browser_->GetProfile()->GetPrefs();

  if (source == start_button_) {
    if (tab_groups::IsProjectsPanelFeatureEnabled()) {
      command_id = IDC_PROJECTS_PANEL_TOGGLE_PIN;
      pref_name = prefs::kProjectsPanelPinnedToTabstrip;
      string_id = prefs->GetBoolean(pref_name)
                      ? IDS_PROJECTS_PANEL_BUTTON_CXMENU_UNPIN
                      : IDS_PROJECTS_PANEL_BUTTON_CXMENU_PIN;
      element_id = kProjectsPanelUnpinMenuItem;
    } else {
      command_id = IDC_EVERYTHING_MENU_TOGGLE_PIN;
      pref_name = prefs::kEverythingMenuPinnedToTabstrip;
      string_id = prefs->GetBoolean(pref_name)
                      ? IDS_EVERYTHING_MENU_BUTTON_CXMENU_UNPIN
                      : IDS_EVERYTHING_MENU_BUTTON_CXMENU_PIN;
      element_id = kEverythingMenuUnpinMenuItem;
    }
  } else if (source == end_button_) {
    command_id = IDC_TAB_SEARCH_TOGGLE_PIN;
    pref_name = prefs::kTabSearchPinnedToTabstrip;
    string_id = prefs->GetBoolean(pref_name)
                    ? IDS_TAB_SEARCH_BUTTON_CXMENU_UNPIN
                    : IDS_TAB_SEARCH_BUTTON_CXMENU_PIN;
    element_id = kTabSearchUnpinMenuItem;
  } else {
    return;
  }

  const bool is_pinned = prefs->GetBoolean(pref_name);
  const gfx::VectorIcon& icon = is_pinned ? kKeepOffIcon : kKeepIcon;

  menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  menu_model_->AddItemWithStringIdAndIcon(
      command_id, string_id,
      ui::ImageModel::FromVectorIcon(icon, ui::kColorIcon, 16));
  menu_model_->SetElementIdentifierAt(0, element_id);

  menu_model_adapter_ = std::make_unique<views::MenuModelAdapter>(
      menu_model_.get(), base::BindRepeating(&TabStripComboButton::OnMenuClosed,
                                             base::Unretained(this)));
  menu_model_adapter_->set_triggerable_event_flags(ui::EF_LEFT_MOUSE_BUTTON |
                                                   ui::EF_RIGHT_MOUSE_BUTTON);
  std::unique_ptr<views::MenuItemView> root = menu_model_adapter_->CreateMenu();
  menu_runner_ = std::make_unique<views::MenuRunner>(
      std::move(root),
      views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU);
  menu_runner_->RunMenuAt(GetWidget(), nullptr,
                          source->GetAnchorBoundsInScreen(),
                          views::MenuAnchorPosition::kTopLeft, source_type);
}

void TabStripComboButton::ExecuteCommand(int command_id, int event_flags) {
  PrefService* prefs = browser_->GetProfile()->GetPrefs();
  std::string_view pref_name;
  if (command_id == IDC_TAB_SEARCH_TOGGLE_PIN) {
    pref_name = prefs::kTabSearchPinnedToTabstrip;
    show_tab_search_ephemerally_ = false;
    hide_tab_search_timer_.Stop();
  } else if (command_id == IDC_PROJECTS_PANEL_TOGGLE_PIN) {
    pref_name = prefs::kProjectsPanelPinnedToTabstrip;
  } else if (command_id == IDC_EVERYTHING_MENU_TOGGLE_PIN) {
    pref_name = prefs::kEverythingMenuPinnedToTabstrip;
  } else {
    return;
  }
  prefs->SetBoolean(pref_name, !prefs->GetBoolean(pref_name));
}

void TabStripComboButton::OnBubbleInitializing() {
  PrefService* prefs = browser_->GetProfile()->GetPrefs();
  if (prefs->GetBoolean(prefs::kTabSearchPinnedToTabstrip)) {
    return;
  }

  show_tab_search_ephemerally_ = true;
  UpdateButtonsVisibility();
}

void TabStripComboButton::OnBubbleDestroying() {
  PrefService* prefs = browser_->GetProfile()->GetPrefs();
  if (prefs->GetBoolean(prefs::kTabSearchPinnedToTabstrip)) {
    return;
  }

  // Post a delayed task to give a chance for the user to use the context menu
  hide_tab_search_timer_.Start(
      FROM_HERE, kHideTabSearchButtonDelay,
      base::BindOnce(&TabStripComboButton::MaybeHideTabSearchButton,
                     base::Unretained(this)));
}

void TabStripComboButton::OnHostDestroying() {
  tab_search_bubble_host_observation_.Reset();
}

void TabStripComboButton::SetTabSearchBubbleHost(TabSearchBubbleHost* host) {
  tab_search_bubble_host_observation_.Reset();
  if (host) {
    tab_search_bubble_host_observation_.Observe(host);
  }
}

void TabStripComboButton::MaybeHideTabSearchButton() {
  PrefService* prefs = browser_->GetProfile()->GetPrefs();

  if (prefs->GetBoolean(prefs::kTabSearchPinnedToTabstrip) ||
      (menu_runner_ && menu_runner_->IsRunning())) {
    return;
  }

  show_tab_search_ephemerally_ = false;
  UpdateButtonsVisibility();
}

actions::ActionItem* TabStripComboButton::GetStartButtonActionItem() {
  const actions::ActionId start_action_id =
      tab_groups::IsProjectsPanelFeatureEnabled() ? kActionToggleProjectsPanel
                                                  : kActionTabGroupsMenu;
  return actions::ActionManager::Get().FindAction(
      start_action_id, browser_->GetActions()->root_action_item());
}

actions::ActionItem* TabStripComboButton::GetEndButtonActionItem() {
  return actions::ActionManager::Get().FindAction(
      kActionTabSearch, browser_->GetActions()->root_action_item());
}

void TabStripComboButton::AnimationProgressed(const gfx::Animation* animation) {
  const double value = animation->GetCurrentValue();

  // Mapping overlapping stages where each lasts for 1/2 of the total duration.
  // 1. Expansion: Starts at 0.0, Ends at 0.5.
  // 2. Opacity: Starts at 0.25, Ends at 0.75.
  // 3. Radius: Starts at 0.5, Ends at 1.0.
  // These formulas work for both appearing (0.0 -> 1.0) and disappearing (1.0
  // -> 0.0).
  const float expansion_factor =
      std::clamp(static_cast<float>(value * 2.0), 0.0f, 1.0f);
  const float opacity_factor =
      std::clamp(static_cast<float>((value - 0.25) * 2.0), 0.0f, 1.0f);
  const float radius_factor =
      std::clamp(static_cast<float>((value - 0.5) * 2.0), 0.0f, 1.0f);

  auto update_buttons = [&](TabStripFlatEdgeButton* primary_button,
                            TabStripFlatEdgeButton* sibling_button) {
    if (!primary_button) {
      return;
    }
    primary_button->SetExpansionFactor(expansion_factor);
    primary_button->SetIconOpacity(opacity_factor);
    primary_button->SetFlatEdgeFactor(radius_factor);
    if (sibling_button) {
      sibling_button->SetFlatEdgeFactor(radius_factor);
    }
  };

  if (animation == &start_button_animation_) {
    update_buttons(start_button_, end_button_);
  } else if (animation == &end_button_animation_) {
    update_buttons(end_button_, start_button_);
  }
}

void TabStripComboButton::AnimationEnded(const gfx::Animation* animation) {
  AnimationProgressed(animation);
  if (animation->GetCurrentValue() == 0.0) {
    if (animation == &start_button_animation_) {
      GetStartButtonActionItem()->SetVisible(false);
    } else if (animation == &end_button_animation_) {
      GetEndButtonActionItem()->SetVisible(false);
    }
  }
}

gfx::Size TabStripComboButton::GetPreferredSizeForOrientation(
    views::LayoutOrientation orientation) {
  int width = 0;
  int height = 0;
  const int spacing =
      GetLayoutConstant(LayoutConstant::kVerticalTabStripFlatEdgeButtonPadding);
  bool has_visible_child = false;

  for (views::View* child : children()) {
    if (!child->GetVisible()) {
      continue;
    }

    gfx::Size child_size = child->GetPreferredSize();

    if (orientation == views::LayoutOrientation::kHorizontal) {
      if (has_visible_child) {
        width += spacing;
      }
      width += child_size.width();
      height = std::max(height, child_size.height());
    } else {
      if (has_visible_child) {
        width += spacing;
      }
      height += child_size.height();
      width = std::max(width, child_size.width());
    }
    has_visible_child = true;
  }

  return gfx::Size(width, height);
}

void TabStripComboButton::OnMenuClosed() {
  menu_runner_.reset();
  if (show_tab_search_ephemerally_) {
    hide_tab_search_timer_.Start(
        FROM_HERE, kHideTabSearchButtonDelay,
        base::BindOnce(&TabStripComboButton::MaybeHideTabSearchButton,
                       base::Unretained(this)));
  }
}

BEGIN_METADATA(TabStripComboButton)
END_METADATA
