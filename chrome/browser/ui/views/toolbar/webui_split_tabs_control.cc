// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_split_tabs_control.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/split_tab_menu_model.h"
#include "chrome/browser/ui/tabs/split_tab_util.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/toolbar/pinned_action_toolbar_button_menu_model.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/browser/ui/webui/webui_toolbar/utils/split_tabs_utils.h"
#include "chrome/browser/ui/webui/webui_toolbar/utils/toolbar_button_utils.h"
#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_ui.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/base_window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/widget/widget.h"

WebUISplitTabsControl::WebUISplitTabsControl(
    WebUIToolbarControlDelegate* delegate)
    : delegate_(delegate) {}

WebUISplitTabsControl::~WebUISplitTabsControl() {
  if (delegate_->GetBrowser() && delegate_->GetBrowser()->GetTabStripModel()) {
    delegate_->GetBrowser()->GetTabStripModel()->RemoveObserver(this);
  }
}

void WebUISplitTabsControl::Init() {
  BrowserWindowInterface* browser = delegate_->GetBrowser();
  pin_state_.Init(prefs::kPinSplitTabButton, browser->GetProfile()->GetPrefs(),
                  base::BindRepeating(&WebUISplitTabsControl::UpdateState,
                                      base::Unretained(this)));

  if (auto* tab_strip_model = browser->GetTabStripModel()) {
    tab_strip_model->AddObserver(this);
  }

  UpdateState();
}

bool WebUISplitTabsControl::IsVisible() const {
  return is_visible_;
}

void WebUISplitTabsControl::HandleContextMenu(
    toolbar_ui_api::mojom::ContextMenuType menu_type,
    const gfx::Rect& screen_rect,
    ui::mojom::MenuSourceType source_type) {
  if (menu_runner_ && menu_runner_->IsRunning()) {
    menu_runner_->Cancel();
    return;
  }
  BrowserWindowInterface* browser = delegate_->GetBrowser();

  if (menu_type == toolbar_ui_api::mojom::ContextMenuType::kSplitTabsAction) {
    // Only show "Separate Views" menu if actually in split.
    auto* tab_strip_model = browser->GetTabStripModel();
    if (!tab_strip_model || !tab_strip_model->GetActiveTab() ||
        !tab_strip_model->GetActiveTab()->IsSplit()) {
      return;
    }
    // Destroy the old menu runner first to avoid a dangling pointer since it
    // holds a raw_ptr to the old menu model.
    menu_runner_.reset();
    split_tab_menu_ = std::make_unique<SplitTabMenuModel>(
        tab_strip_model, SplitTabMenuModel::MenuSource::kToolbarButton);
    RunMenuAt(screen_rect, source_type, /*is_action_menu=*/true);
  } else if (menu_type ==
             toolbar_ui_api::mojom::ContextMenuType::kSplitTabsContext) {
    // Destroy the old menu runner first to avoid a dangling pointer since it
    // holds a raw_ptr to the old menu model.
    menu_runner_.reset();
    split_tab_menu_ = std::make_unique<PinnedActionToolbarButtonMenuModel>(
        browser, kActionSplitTab);
    RunMenuAt(screen_rect, source_type, /*is_action_menu=*/false);
  }
}

void WebUISplitTabsControl::RunMenuAt(const gfx::Rect& screen_rect,
                                      ui::mojom::MenuSourceType source_type,
                                      bool is_action_menu) {
  last_source_type_for_testing_ = source_type;
  if (is_action_menu) {
    menu_runner_ = std::make_unique<views::MenuRunner>(
        split_tab_menu_.get(), views::MenuRunner::HAS_MNEMONICS,
        base::BindRepeating(&WebUISplitTabsControl::UpdateState,
                            base::Unretained(this)));
  } else {
    menu_model_adapter_ = std::make_unique<views::MenuModelAdapter>(
        split_tab_menu_.get(),
        base::BindRepeating(&WebUISplitTabsControl::UpdateState,
                            base::Unretained(this)));
    std::unique_ptr<views::MenuItemView> root =
        menu_model_adapter_->CreateMenu();

    menu_runner_ = std::make_unique<views::MenuRunner>(
        std::move(root), views::MenuRunner::HAS_MNEMONICS);
  }

  menu_runner_->RunMenuAt(delegate_->GetView()->GetWidget(), nullptr,
                          screen_rect, views::MenuAnchorPosition::kTopLeft,
                          source_type);
  UpdateState();
}

void WebUISplitTabsControl::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (selection.active_tab_changed()) {
    // Force the menu to close if it's open because the active tab may have
    // changed and invalidated the menu.
    if (menu_runner_ && menu_runner_->IsRunning()) {
      menu_runner_->Cancel();
    }
    UpdateState();
  }
}

void WebUISplitTabsControl::OnSplitTabChanged(const SplitTabChange& change) {
  if (change.type == SplitTabChange::Type::kAdded ||
      change.type == SplitTabChange::Type::kRemoved ||
      change.type == SplitTabChange::Type::kContentsChanged) {
    UpdateState();
  }
}

void WebUISplitTabsControl::UpdateVisibility(
    const toolbar_ui_api::mojom::SplitTabsControlState* state) {
  bool should_be_visible = state->is_pinned || state->is_current_tab_split;

  if (should_be_visible != is_visible_) {
    is_visible_ = should_be_visible;
    delegate_->OnPreferredSizeChanged();
  }
}

void WebUISplitTabsControl::UpdateState() {
  auto state = toolbar_ui_api::mojom::SplitTabsControlState::New();
  auto s = webui_toolbar::ComputeTabSplitStatus(delegate_->GetBrowser());
  state->is_current_tab_split = s.is_split;
  state->location = s.location;
  state->is_pinned = pin_state_.GetValue();
  state->is_context_menu_visible = menu_runner_ && menu_runner_->IsRunning();
  UpdateVisibility(state.get());
  delegate_->OnSplitTabsControlStateChanged(std::move(state));
}
