// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/shared/tab_strip_combo_button.h"

#include "base/i18n/rtl.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
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
#include "ui/views/view_class_properties.h"

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TabStripComboButton,
                                      kTabSearchUnpinMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TabStripComboButton,
                                      kProjectsPanelUnpinMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TabStripComboButton,
                                      kEverythingMenuUnpinMenuItem);

TabStripComboButton::TabStripComboButton(BrowserWindowInterface* browser)
    : browser_(browser),
      action_view_controller_(std::make_unique<views::ActionViewController>()) {
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

TabStripComboButton::~TabStripComboButton() = default;

void TabStripComboButton::UpdateButtonsVisibility() {
  if (!browser_ || !browser_->GetActions()) {
    return;
  }

  PrefService* prefs = browser_->GetProfile()->GetPrefs();
  const actions::ActionId start_action_id =
      tab_groups::IsProjectsPanelFeatureEnabled() ? kActionToggleProjectsPanel
                                                  : kActionTabGroupsMenu;
  const std::string_view pref_name =
      tab_groups::IsProjectsPanelFeatureEnabled()
          ? prefs::kProjectsPanelPinnedToTabstrip
          : prefs::kEverythingMenuPinnedToTabstrip;
  actions::ActionItem* start_action_item =
      actions::ActionManager::Get().FindAction(
          start_action_id, browser_->GetActions()->root_action_item());
  if (start_action_item) {
    start_action_item->SetVisible(prefs->GetBoolean(pref_name));
  }

  actions::ActionItem* end_action_item =
      actions::ActionManager::Get().FindAction(
          kActionTabSearch, browser_->GetActions()->root_action_item());
  if (end_action_item) {
    end_action_item->SetVisible(
        prefs->GetBoolean(prefs::kTabSearchPinnedToTabstrip));
  }
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

  const int raw_button_size = GetLayoutConstant(
      LayoutConstant::kVerticalTabStripTopContainerButtonSize);
  button->SetPreferredSize(gfx::Size(raw_button_size, raw_button_size));

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

  if (source == start_button_) {
    if (tab_groups::IsProjectsPanelFeatureEnabled()) {
      command_id = IDC_PROJECTS_PANEL_TOGGLE_PIN;
      string_id = IDS_PROJECTS_PANEL_BUTTON_CXMENU_UNPIN;
      element_id = kProjectsPanelUnpinMenuItem;
    } else {
      command_id = IDC_EVERYTHING_MENU_TOGGLE_PIN;
      string_id = IDS_EVERYTHING_MENU_BUTTON_CXMENU_UNPIN;
      element_id = kEverythingMenuUnpinMenuItem;
    }
  } else if (source == end_button_) {
    command_id = IDC_TAB_SEARCH_TOGGLE_PIN;
    string_id = IDS_TAB_SEARCH_BUTTON_CXMENU_UNPIN;
    element_id = kTabSearchUnpinMenuItem;
  } else {
    return;
  }

  menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  menu_model_->AddItemWithStringIdAndIcon(
      command_id, string_id,
      ui::ImageModel::FromVectorIcon(kKeepOffIcon, ui::kColorIcon, 16));
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
  if (command_id == IDC_TAB_SEARCH_TOGGLE_PIN) {
    prefs->SetBoolean(prefs::kTabSearchPinnedToTabstrip, false);
  } else if (command_id == IDC_PROJECTS_PANEL_TOGGLE_PIN) {
    prefs->SetBoolean(prefs::kProjectsPanelPinnedToTabstrip, false);
  } else if (command_id == IDC_EVERYTHING_MENU_TOGGLE_PIN) {
    prefs->SetBoolean(prefs::kEverythingMenuPinnedToTabstrip, false);
  }
}

void TabStripComboButton::OnMenuClosed() {
  menu_runner_.reset();
}

BEGIN_METADATA(TabStripComboButton)
END_METADATA
