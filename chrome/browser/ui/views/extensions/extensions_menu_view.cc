// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_view.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/views/bubble_menu_item_factory.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_item_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/ink_drop_host_view.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"

namespace {
// If true, allows more than one instance of the ExtensionsMenuView, which may
// not be the active instance in g_extensions_dialog.
bool g_allow_testing_dialogs = false;

ExtensionsMenuView* g_extensions_dialog = nullptr;

constexpr int EXTENSIONS_SETTINGS_ID = 42;
}  // namespace

constexpr gfx::Size ExtensionsMenuView::kExtensionsMenuIconSize;

ExtensionsMenuView::ButtonListener::ButtonListener(Browser* browser)
    : browser_(browser) {}

void ExtensionsMenuView::ButtonListener::ButtonPressed(views::Button* sender,
                                                       const ui::Event& event) {
  DCHECK_EQ(sender->GetID(), EXTENSIONS_SETTINGS_ID);
  chrome::ShowExtensions(browser_, std::string());
}

ExtensionsMenuView::ExtensionsMenuView(
    views::View* anchor_view,
    Browser* browser,
    ExtensionsContainer* extensions_container)
    : BubbleDialogDelegateView(anchor_view,
                               views::BubbleBorder::Arrow::TOP_RIGHT),
      browser_(browser),
      extensions_container_(extensions_container),
      model_(ToolbarActionsModel::Get(browser_->profile())),
      model_observer_(this),
      button_listener_(browser_) {
  model_observer_.Add(model_);
  set_margins(gfx::Insets(0));

  DialogDelegate::set_buttons(ui::DIALOG_BUTTON_NONE);

  EnableUpDownKeyboardAccelerators();

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  Repopulate();
}

ExtensionsMenuView::~ExtensionsMenuView() {
  if (!g_allow_testing_dialogs)
    DCHECK_EQ(g_extensions_dialog, this);
  g_extensions_dialog = nullptr;
  extensions_menu_items_.clear();
}

base::string16 ExtensionsMenuView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_EXTENSIONS_MENU_TITLE);
}

bool ExtensionsMenuView::ShouldShowCloseButton() const {
  return true;
}

gfx::Size ExtensionsMenuView::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                        DISTANCE_BUBBLE_PREFERRED_WIDTH) -
                    margins().width();
  return gfx::Size(width, GetHeightForWidth(width));
}

void ExtensionsMenuView::Repopulate() {
  RemoveAllChildViews(true);

  auto extension_buttons = CreateExtensionButtonsContainer();

  constexpr int kMaxExtensionButtonsHeightDp = 600;
  auto scroll_view = std::make_unique<views::ScrollView>();
  scroll_view->ClipHeightTo(0, kMaxExtensionButtonsHeightDp);
  scroll_view->SetDrawOverflowIndicator(false);
  scroll_view->SetHideHorizontalScrollBar(true);
  scroll_view->SetContents(std::move(extension_buttons));
  AddChildView(std::move(scroll_view));

  AddChildView(std::make_unique<views::Separator>());

  auto footer = CreateBubbleMenuItem(
      EXTENSIONS_SETTINGS_ID, l10n_util::GetStringUTF16(IDS_MANAGE_EXTENSION),
      &button_listener_);
  footer->SetImage(
      views::Button::STATE_NORMAL,
      gfx::CreateVectorIcon(vector_icons::kSettingsIcon, 16,
                            GetNativeTheme()->GetSystemColor(
                                ui::NativeTheme::kColorId_DefaultIconColor)));
  footer->SetImageLabelSpacing(ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUTTON_HORIZONTAL_PADDING));
  manage_extensions_button_for_testing_ = footer.get();
  AddChildView(std::move(footer));
}

std::unique_ptr<views::View>
ExtensionsMenuView::CreateExtensionButtonsContainer() {
  extensions_menu_items_.clear();
  content::WebContents* const web_contents =
      browser_->tab_strip_model()->GetActiveWebContents();

  // Group actions by access levels.
  std::vector<std::unique_ptr<ToolbarActionViewController>> cant_access;
  std::vector<std::unique_ptr<ToolbarActionViewController>> wants_access;
  std::vector<std::unique_ptr<ToolbarActionViewController>> accessing_site_data;
  for (auto action_id : model_->action_ids()) {
    auto action = model_->CreateActionForId(browser_, extensions_container_,
                                            false, action_id);
    switch (action->GetPageInteractionStatus(web_contents)) {
      case ToolbarActionViewController::PageInteractionStatus::kNone:
        cant_access.push_back(std::move(action));
        break;
      case ToolbarActionViewController::PageInteractionStatus::kPending:
        wants_access.push_back(std::move(action));
        break;
      case ToolbarActionViewController::PageInteractionStatus::kActive:
        accessing_site_data.push_back(std::move(action));
        break;
    }
    // Action should be moved into one of the groups.
    DCHECK(!action);
  }

  auto extension_buttons = std::make_unique<views::View>();
  extension_buttons->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  auto add_group =
      [this, &extension_buttons](
          std::vector<std::unique_ptr<ToolbarActionViewController>>*
              controller_group,
          int label_string_id) {
        if (controller_group->empty())
          return;

        // Add a label as header for non-empty groups of items.
        auto label = std::make_unique<views::Label>(
            l10n_util::GetStringUTF16(label_string_id),
            ChromeTextContext::CONTEXT_BODY_TEXT_LARGE,
            views::style::STYLE_SECONDARY);
        label->SetMultiLine(true);
        label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
        const int horizontal_spacing =
            ChromeLayoutProvider::Get()->GetDistanceMetric(
                views::DISTANCE_BUTTON_HORIZONTAL_PADDING);
        label->SetBorder(views::CreateEmptyBorder(
            ChromeLayoutProvider::Get()->GetDistanceMetric(
                DISTANCE_CONTROL_LIST_VERTICAL),
            horizontal_spacing,
            ChromeLayoutProvider::Get()->GetDistanceMetric(
                DISTANCE_RELATED_CONTROL_VERTICAL_SMALL),
            horizontal_spacing));

        extension_buttons->AddChildView(std::move(label));

        // Sort the actions on action name.
        std::sort(
            controller_group->begin(), controller_group->end(),
            [](const std::unique_ptr<ToolbarActionViewController>& a,
               const std::unique_ptr<ToolbarActionViewController>& b) -> bool {
              return a->GetActionName() < b->GetActionName();
            });

        for (auto& controller : *controller_group) {
          std::unique_ptr<ExtensionsMenuItemView> extensions_menu_item =
              std::make_unique<ExtensionsMenuItemView>(browser_,
                                                       std::move(controller));

          extensions_menu_items_.push_back(extensions_menu_item.get());
          extension_buttons->AddChildView(std::move(extensions_menu_item));
        }
        controller_group->clear();
      };

  add_group(&accessing_site_data, IDS_EXTENSIONS_MENU_ACCESSING_SITE_DATA);
  add_group(&wants_access, IDS_EXTENSIONS_MENU_WANTS_TO_ACCESS_SITE_DATA);
  add_group(&cant_access, IDS_EXTENSIONS_MENU_CANT_ACCESS_SITE_DATA);

  return extension_buttons;
}

// TODO(pbos): Revisit observed events below.
void ExtensionsMenuView::OnToolbarActionAdded(
    const ToolbarActionsModel::ActionId& item,
    int index) {
  Repopulate();
}

void ExtensionsMenuView::OnToolbarActionRemoved(
    const ToolbarActionsModel::ActionId& action_id) {
  Repopulate();
}

void ExtensionsMenuView::OnToolbarActionMoved(
    const ToolbarActionsModel::ActionId& action_id,
    int index) {
  Repopulate();
}

void ExtensionsMenuView::OnToolbarActionLoadFailed() {
  Repopulate();
}

void ExtensionsMenuView::OnToolbarActionUpdated(
    const ToolbarActionsModel::ActionId& action_id) {
  Repopulate();
}

void ExtensionsMenuView::OnToolbarVisibleCountChanged() {
  Repopulate();
}

void ExtensionsMenuView::OnToolbarHighlightModeChanged(bool is_highlighting) {
  Repopulate();
}

void ExtensionsMenuView::OnToolbarModelInitialized() {
  Repopulate();
}

void ExtensionsMenuView::OnToolbarPinnedActionsChanged() {
  for (auto* menu_item : extensions_menu_items_) {
    menu_item->UpdatePinButton();
  }
}

// static
base::AutoReset<bool> ExtensionsMenuView::AllowInstancesForTesting() {
  return base::AutoReset<bool>(&g_allow_testing_dialogs, true);
}

// static
void ExtensionsMenuView::ShowBubble(views::View* anchor_view,
                                    Browser* browser,
                                    ExtensionsContainer* extensions_container) {
  DCHECK(!g_extensions_dialog);
  g_extensions_dialog =
      new ExtensionsMenuView(anchor_view, browser, extensions_container);
  views::BubbleDialogDelegateView::CreateBubble(g_extensions_dialog)->Show();
}

// static
bool ExtensionsMenuView::IsShowing() {
  return g_extensions_dialog != nullptr;
}

// static
void ExtensionsMenuView::Hide() {
  if (IsShowing())
    g_extensions_dialog->GetWidget()->Close();
}

ExtensionsMenuView* ExtensionsMenuView::GetExtensionsMenuViewForTesting() {
  return g_extensions_dialog;
}
