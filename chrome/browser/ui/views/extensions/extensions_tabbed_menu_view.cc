// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_tabbed_menu_view.h"

#include <algorithm>
#include <memory>
#include <string>

#include "base/feature_list.h"
#include "base/i18n/case_conversion.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/extension_action_view_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_item_view.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

namespace {
ExtensionsTabbedMenuView* g_extensions_dialog = nullptr;

// Adds a new tab in `tabbed_pane` at `index` with the given `title` and
// `contents`.
void CreateTab(raw_ptr<views::TabbedPane> tabbed_pane,
               size_t index,
               int title_string_id,
               std::unique_ptr<views::View> contents) {
  // This is set so that the extensions menu doesn't fall outside the monitor in
  // a maximized window in 1024x768. See https://crbug.com/1096630.
  // TODO(pbos): Consider making this dynamic and handled by views. Ideally we
  // wouldn't ever pop up so that they pop outside the screen.
  constexpr int kMaxExtensionButtonsHeightDp = 448;
  auto scroll_view = std::make_unique<views::ScrollView>();
  scroll_view->ClipHeightTo(0, kMaxExtensionButtonsHeightDp);
  scroll_view->SetDrawOverflowIndicator(false);
  scroll_view->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  scroll_view->SetContents(std::move(contents));

  tabbed_pane->AddTabAtIndex(index, l10n_util::GetStringUTF16(title_string_id),
                             std::move(scroll_view));
}

// A helper method to convert to an ExtensionsMenuItemView. This cannot be used
// to *determine* if a view is an ExtensionsMenuItemView (it should only be used
// when the view is known to be one). It is only used as an extra measure to
// prevent bad static casts.
ExtensionsMenuItemView* GetAsMenuItemView(views::View* view) {
  DCHECK(views::IsViewClass<ExtensionsMenuItemView>(view));
  return static_cast<ExtensionsMenuItemView*>(view);
}

// Returns the menu item view of `action_id` if it is a children of
// `parent_view`.
ExtensionsMenuItemView* GetMenuItemView(
    views::View* parent_view,
    const ToolbarActionsModel::ActionId& action_id) {
  for (auto* view : parent_view->children()) {
    auto* item_view = GetAsMenuItemView(view);
    if (item_view->view_controller()->GetId() == action_id)
      return item_view;
  }
  return nullptr;
}

// Returns the current index or insert position of `extension_name` in
// `parent_view`, based on alphabetical order.
int FindIndex(const std::u16string extension_name, views::View* parent_view) {
  const auto& children = parent_view->children();
  return std::find_if(children.begin(), children.end(),
                      [extension_name](views::View* v) {
                        return base::i18n::ToLower(extension_name) <=
                               base::i18n::ToLower(GetAsMenuItemView(v)
                                                       ->view_controller()
                                                       ->GetActionName());
                      }) -
         children.begin();
}

// Updates the `item_view` state and its position under `parent_view`.
void UpdateMenuItemView(ExtensionsMenuItemView* item_view,
                        views::View* parent_view) {
  item_view->view_controller()->UpdateState();
  int new_index =
      FindIndex(item_view->view_controller()->GetActionName(), parent_view);
  parent_view->ReorderChildView(item_view, new_index);
}

}  // namespace

ExtensionsTabbedMenuView::ExtensionsTabbedMenuView(
    views::View* anchor_view,
    Browser* browser,
    ExtensionsContainer* extensions_container,
    ExtensionsToolbarButton::ButtonType button_type,
    bool allow_pinning)
    : BubbleDialogDelegateView(anchor_view,
                               views::BubbleBorder::Arrow::TOP_RIGHT),
      browser_(browser),
      extensions_container_(extensions_container),
      toolbar_model_(ToolbarActionsModel::Get(browser_->profile())),
      allow_pinning_(allow_pinning),
      requests_access_{
          nullptr, nullptr,
          IDS_EXTENSIONS_MENU_SITE_ACCESS_TAB_REQUESTS_ACCESS_SECTION_TITLE,
          ToolbarActionViewController::PageInteractionStatus::kPending},
      has_access_{nullptr, nullptr,
                  IDS_EXTENSIONS_MENU_SITE_ACCESS_TAB_HAS_ACCESS_SECTION_TITLE,
                  ToolbarActionViewController::PageInteractionStatus::kActive} {
  // Ensure layer masking is used for the extensions menu to ensure buttons with
  // layer effects sitting flush with the bottom of the bubble are clipped
  // appropriately.
  SetPaintClientToLayer(true);

  toolbar_model_observation_.Observe(toolbar_model_.get());
  browser_->tab_strip_model()->AddObserver(this);
  set_margins(gfx::Insets(0));

  SetButtons(ui::DIALOG_BUTTON_NONE);
  SetShowCloseButton(true);
  SetTitle(IDS_EXTENSIONS_MENU_TITLE);
  GetViewAccessibility().OverrideName(GetAccessibleWindowTitle());

  SetEnableArrowKeyTraversal(true);

  // Let anchor view's MenuButtonController handle the highlight.
  set_highlight_button_when_shown(false);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  Populate();

  // Tabs left to right order is 'site access tab' | 'extensions tab'.
  tabbed_pane_->SelectTabAt(button_type ==
                            ExtensionsToolbarButton::ButtonType::kExtensions);
}

ExtensionsTabbedMenuView::~ExtensionsTabbedMenuView() {
  g_extensions_dialog = nullptr;

  // Note: No need to call TabStripModel::RemoveObserver(), because it's handled
  // directly within TabStripModelObserver::~TabStripModelObserver().
}

// static
views::Widget* ExtensionsTabbedMenuView::ShowBubble(
    views::View* anchor_view,
    Browser* browser,
    ExtensionsContainer* extensions_container_,
    ExtensionsToolbarButton::ButtonType button_type,
    bool allow_pining) {
  DCHECK(!g_extensions_dialog);
  DCHECK(base::FeatureList::IsEnabled(features::kExtensionsMenuAccessControl));
  g_extensions_dialog = new ExtensionsTabbedMenuView(
      anchor_view, browser, extensions_container_, button_type, allow_pining);
  views::Widget* widget =
      views::BubbleDialogDelegateView::CreateBubble(g_extensions_dialog);
  widget->Show();
  return widget;
}

// static
bool ExtensionsTabbedMenuView::IsShowing() {
  return g_extensions_dialog != nullptr;
}

// static
void ExtensionsTabbedMenuView::Hide() {
  DCHECK(base::FeatureList::IsEnabled(features::kExtensionsMenuAccessControl));
  if (IsShowing()) {
    g_extensions_dialog->GetWidget()->Close();
    // Set the dialog to nullptr since `GetWidget->Close()` is not synchronous.
    g_extensions_dialog = nullptr;
  }
}

// static
ExtensionsTabbedMenuView*
ExtensionsTabbedMenuView::GetExtensionsTabbedMenuViewForTesting() {
  return g_extensions_dialog;
}

std::vector<ExtensionsMenuItemView*>
ExtensionsTabbedMenuView::GetInstalledItemsForTesting() const {
  std::vector<ExtensionsMenuItemView*> menu_item_views;
  if (IsShowing()) {
    for (views::View* view : installed_items_->children())
      menu_item_views.push_back(GetAsMenuItemView(view));
  }
  return menu_item_views;
}

std::vector<ExtensionsMenuItemView*>
ExtensionsTabbedMenuView::GetHasAccessItemsForTesting() const {
  std::vector<ExtensionsMenuItemView*> menu_item_views;
  if (IsShowing()) {
    for (views::View* view : has_access_.items->children())
      menu_item_views.push_back(GetAsMenuItemView(view));
  }
  return menu_item_views;
}

std::vector<ExtensionsMenuItemView*>
ExtensionsTabbedMenuView::GetRequestsAccessItemsForTesting() const {
  std::vector<ExtensionsMenuItemView*> menu_item_views;
  if (IsShowing()) {
    for (views::View* view : requests_access_.items->children())
      menu_item_views.push_back(GetAsMenuItemView(view));
  }
  return menu_item_views;
}

size_t ExtensionsTabbedMenuView::GetSelectedTabIndex() const {
  return tabbed_pane_->GetSelectedTabIndex();
}

std::u16string ExtensionsTabbedMenuView::GetAccessibleWindowTitle() const {
  // The title is already spoken via the call to SetTitle().
  return std::u16string();
}

void ExtensionsTabbedMenuView::TabChangedAt(content::WebContents* contents,
                                            int index,
                                            TabChangeType change_type) {
  Update();
}

void ExtensionsTabbedMenuView::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  Update();
}

void ExtensionsTabbedMenuView::OnToolbarActionAdded(
    const ToolbarActionsModel::ActionId& action_id) {
  auto extension_name = toolbar_model_->GetExtensionName(action_id);
  auto index = FindIndex(extension_name, installed_items_);
  CreateAndInsertInstalledExtension(action_id, index);

  MaybeCreateAndInsertSiteAccessItem(action_id);
  UpdateSiteAccessSectionsVisibility();

  ConsistencyCheck();
}

void ExtensionsTabbedMenuView::OnToolbarActionRemoved(
    const ToolbarActionsModel::ActionId& action_id) {
  auto remove_item = [](views::View* parent_view,
                        const ToolbarActionsModel::ActionId& action_id) {
    auto* item_view = GetMenuItemView(parent_view, action_id);
    if (item_view)
      parent_view->RemoveChildViewT(item_view);
  };

  remove_item(installed_items_, action_id);
  remove_item(requests_access_.items, action_id);
  remove_item(has_access_.items, action_id);

  UpdateSiteAccessSectionsVisibility();

  ConsistencyCheck();
}

void ExtensionsTabbedMenuView::OnToolbarActionUpdated(
    const ToolbarActionsModel::ActionId& action_id) {
  auto update_item = [](views::View* parent_view,
                        const ToolbarActionsModel::ActionId& action_id) {
    auto* item_view = GetMenuItemView(parent_view, action_id);
    if (item_view)
      UpdateMenuItemView(item_view, parent_view);
  };

  update_item(installed_items_, action_id);
  update_item(requests_access_.items, action_id);
  update_item(has_access_.items, action_id);

  MoveItemsBetweenSectionsIfNecessary();

  UpdateSiteAccessSectionsVisibility();

  ConsistencyCheck();
}

void ExtensionsTabbedMenuView::OnToolbarModelInitialized() {
  DCHECK(installed_items_->children().empty());
  DCHECK(requests_access_.items->children().empty());
  DCHECK(has_access_.items->children().empty());
  Populate();
}

void ExtensionsTabbedMenuView::OnToolbarPinnedActionsChanged() {
  for (views::View* view : installed_items_->children())
    GetAsMenuItemView(view)->UpdatePinButton();
}

void ExtensionsTabbedMenuView::Populate() {
  // The actions for the profile haven't been initialized yet. We'll call in
  // again once they have.
  if (!toolbar_model_->actions_initialized())
    return;

  DCHECK(children().empty()) << "Populate() can only be called once!";

  tabbed_pane_ = AddChildView(std::make_unique<views::TabbedPane>());
  tabbed_pane_->SetFocusBehavior(views::View::FocusBehavior::NEVER);

  CreateTab(tabbed_pane_, 0, IDS_EXTENSIONS_MENU_SITE_ACCESS_TAB_TITLE,
            CreateSiteAccessContainer());

  auto installed_items = std::make_unique<views::View>();
  installed_items->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  installed_items_ = installed_items.get();
  CreateTab(tabbed_pane_, 1, IDS_EXTENSIONS_MENU_EXTENSIONS_TAB_TITLE,
            std::move(installed_items));

  // Sort action ids based on their extension name.
  auto sort_by_name = [this](const ToolbarActionsModel::ActionId a,
                             const ToolbarActionsModel::ActionId b) {
    return base::i18n::ToLower(toolbar_model_->GetExtensionName(a)) <
           base::i18n::ToLower(toolbar_model_->GetExtensionName(b));
  };
  std::vector<std::string> sorted_ids(toolbar_model_->action_ids().begin(),
                                      toolbar_model_->action_ids().end());
  std::sort(sorted_ids.begin(), sorted_ids.end(), sort_by_name);

  for (size_t i = 0; i < sorted_ids.size(); ++i) {
    CreateAndInsertInstalledExtension(sorted_ids[i], i);
    MaybeCreateAndInsertSiteAccessItem(sorted_ids[i]);
  }

  UpdateSiteAccessSectionsVisibility();

  ConsistencyCheck();
}

void ExtensionsTabbedMenuView::Update() {
  auto update_items = [](views::View* parent_view) {
    for (views::View* view : parent_view->children()) {
      auto* item_view = GetAsMenuItemView(view);
      UpdateMenuItemView(item_view, parent_view);
    }
  };

  update_items(installed_items_);
  update_items(requests_access_.items);
  update_items(has_access_.items);

  MoveItemsBetweenSectionsIfNecessary();

  UpdateSiteAccessSectionsVisibility();
}

std::unique_ptr<views::View>
ExtensionsTabbedMenuView::CreateSiteAccessContainer() {
  auto site_access_container = std::make_unique<views::View>();
  site_access_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  auto current_site = base::UTF8ToUTF16(browser_->tab_strip_model()
                                            ->GetActiveWebContents()
                                            ->GetLastCommittedURL()
                                            .host());

  auto create_section =
      [current_site](ExtensionsTabbedMenuView::SiteAccessSection* section) {
        auto section_container = std::make_unique<views::View>();
        section->container = section_container.get();
        section_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kVertical));

        const int horizontal_spacing =
            ChromeLayoutProvider::Get()->GetDistanceMetric(
                views::DISTANCE_BUTTON_HORIZONTAL_PADDING);

        // Add an emphasized short header explaining the section.
        auto header = std::make_unique<views::Label>(
            l10n_util::GetStringFUTF16(section->header_string_id, current_site),
            ChromeTextContext::CONTEXT_DIALOG_BODY_TEXT_SMALL,
            ChromeTextStyle::STYLE_EMPHASIZED);
        header->SetHorizontalAlignment(gfx::ALIGN_LEFT);
        header->SetBorder(views::CreateEmptyBorder(
            ChromeLayoutProvider::Get()->GetDistanceMetric(
                DISTANCE_CONTROL_LIST_VERTICAL),
            horizontal_spacing, 0, horizontal_spacing));
        section_container->AddChildView(std::move(header));

        // Add an empty section for the menu items of the section. Items will be
        // populated later.
        auto items = std::make_unique<views::View>();
        items->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kVertical));
        section->items = items.get();
        section_container->AddChildView(std::move(items));

        // Start off with the section invisible. We'll update it as we add items
        // if necessary.
        section_container->SetVisible(false);

        return section_container;
      };

  site_access_container->AddChildView(create_section(&requests_access_));
  site_access_container->AddChildView(create_section(&has_access_));

  return site_access_container;
}

void ExtensionsTabbedMenuView::CreateAndInsertInstalledExtension(
    const ToolbarActionsModel::ActionId& id,
    int index) {
  std::unique_ptr<ExtensionActionViewController> controller =
      ExtensionActionViewController::Create(id, browser_,
                                            extensions_container_);
  auto item = std::make_unique<ExtensionsMenuItemView>(
      ExtensionsMenuItemView::MenuItemType::kExtensions, browser_,
      std::move(controller), allow_pinning_);
  installed_items_->AddChildViewAt(std::move(item), index);
}

void ExtensionsTabbedMenuView::MaybeCreateAndInsertSiteAccessItem(
    const ToolbarActionsModel::ActionId& id) {
  std::unique_ptr<ExtensionActionViewController> controller =
      ExtensionActionViewController::Create(id, browser_,
                                            extensions_container_);

  // Extensions with no current site interaction don't belong to a site access
  // section and therefore do not need a site access item view.
  const ToolbarActionViewController::PageInteractionStatus status =
      controller->GetPageInteractionStatus(
          browser_->tab_strip_model()->GetActiveWebContents());
  auto* section = GetSiteAccessSectionForPageStatus(status);
  if (!section)
    return;

  auto item = std::make_unique<ExtensionsMenuItemView>(
      ExtensionsMenuItemView::MenuItemType::kSiteAccess, browser_,
      std::move(controller), allow_pinning_);

  InsertSiteAccessItem(std::move(item), section);
}

void ExtensionsTabbedMenuView::InsertSiteAccessItem(
    std::unique_ptr<ExtensionsMenuItemView> item,
    SiteAccessSection* section) {
  DCHECK(section);

  int index =
      FindIndex(item->view_controller()->GetActionName(), section->items);
  section->items->AddChildViewAt(std::move(item), index);
}

void ExtensionsTabbedMenuView::MoveItemsBetweenSectionsIfNecessary() {
  content::WebContents* const web_contents =
      browser_->tab_strip_model()->GetActiveWebContents();

  auto move_items_between_sections_if_necessary =
      [web_contents, this](SiteAccessSection* section) {
        // Collect the views to move separately, so that we don't change the
        // children of the view during iteration.
        std::vector<ExtensionsMenuItemView*> items_to_move;
        for (views::View* view : section->items->children()) {
          auto* item_view = GetAsMenuItemView(view);
          auto item_page_status =
              item_view->view_controller()->GetPageInteractionStatus(
                  web_contents);
          if (item_page_status != section->page_status)
            items_to_move.push_back(item_view);
        }

        for (ExtensionsMenuItemView* item_view : items_to_move) {
          auto item_view_to_move = section->items->RemoveChildViewT(item_view);
          auto* new_section = GetSiteAccessSectionForPageStatus(
              item_view_to_move->view_controller()->GetPageInteractionStatus(
                  web_contents));
          if (!new_section)
            return;

          InsertSiteAccessItem(std::move(item_view_to_move), new_section);
        }
      };

  move_items_between_sections_if_necessary(&requests_access_);
  move_items_between_sections_if_necessary(&has_access_);
}

void ExtensionsTabbedMenuView::UpdateSiteAccessSectionsVisibility() {
  auto update_section = [](SiteAccessSection* section) {
    bool should_be_visible = !section->items->children().empty();
    if (section->container->GetVisible() != should_be_visible)
      section->container->SetVisible(should_be_visible);
  };

  update_section(&has_access_);
  update_section(&requests_access_);

  // TODO(crbug.com/1263310): If no extensions have or request access to the
  // current site, show respective message.

  // TODO(crbug.com/1263310): If user is on a chrome:-scheme page, show
  // respective message.
}

ExtensionsTabbedMenuView::SiteAccessSection*
ExtensionsTabbedMenuView::GetSiteAccessSectionForPageStatus(
    ToolbarActionViewController::PageInteractionStatus status) {
  switch (status) {
    case ToolbarActionViewController::PageInteractionStatus::kNone:
      // Extensions with no interaction with the current site don't belong to a
      // site access section.
      return nullptr;
    case ToolbarActionViewController::PageInteractionStatus::kPending:
      return &requests_access_;
    case ToolbarActionViewController::PageInteractionStatus::kActive:
      return &has_access_;
  }
}

void ExtensionsTabbedMenuView::ConsistencyCheck() {
#if DCHECK_IS_ON()
  const base::flat_set<std::string>& action_ids = toolbar_model_->action_ids();

  // Check that all items are owned by the view hierarchy, and that each
  // corresponds to an item in the model.
  std::vector<std::u16string> installed_items_names;
  for (views::View* view : installed_items_->children()) {
    DCHECK(Contains(view));
    auto* installed_item_view = GetAsMenuItemView(view);
    DCHECK(base::Contains(action_ids,
                          installed_item_view->view_controller()->GetId()));
    installed_items_names.push_back(base::i18n::ToLower(
        installed_item_view->view_controller()->GetActionName()));
  }

  // Verify that all installed extensions are properly sorted.
  DCHECK(std::is_sorted(installed_items_names.begin(),
                        installed_items_names.end()));
#endif
}

BEGIN_METADATA(ExtensionsTabbedMenuView, views::BubbleDialogDelegateView)
END_METADATA
