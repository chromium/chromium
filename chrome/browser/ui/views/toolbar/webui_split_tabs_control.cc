// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_split_tabs_control.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/split_tab_menu_model.h"
#include "chrome/browser/ui/tabs/split_tab_util.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/toolbar/pinned_action_toolbar_button_menu_model.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/browser/ui/webui/webui_toolbar/utils/split_tabs_utils.h"
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
#include "ui/gfx/geometry/point.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/widget/widget.h"

WebUISplitTabsControl::WebUISplitTabsControl(WebUIToolbarWebView* toolbar_view)
    : toolbar_view_(toolbar_view) {}

WebUISplitTabsControl::~WebUISplitTabsControl() {
  if (toolbar_view_->browser_ && toolbar_view_->browser_->GetTabStripModel()) {
    toolbar_view_->browser_->GetTabStripModel()->RemoveObserver(this);
  }
}

void WebUISplitTabsControl::Init() {
  BrowserWindowInterface* browser = toolbar_view_->browser_;
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
    const gfx::Point& screen_location,
    ui::mojom::MenuSourceType source_type) {
  BrowserWindowInterface* browser = toolbar_view_->browser_;
  current_menu_type_ = menu_type;
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
    RunMenuAt(screen_location.x(), screen_location.y(), source_type);
  } else if (menu_type ==
             toolbar_ui_api::mojom::ContextMenuType::kSplitTabsContext) {
    Browser* actual_browser =
        chrome::FindBrowserWithWindow(browser->GetWindow()->GetNativeWindow());
    if (actual_browser) {
      // Destroy the old menu runner first to avoid a dangling pointer since it
      // holds a raw_ptr to the old menu model.
      menu_runner_.reset();
      split_tab_menu_ = std::make_unique<PinnedActionToolbarButtonMenuModel>(
          actual_browser, kActionSplitTab);
      RunMenuAt(screen_location.x(), screen_location.y(), source_type);
    }
  }
}

void WebUISplitTabsControl::RunMenuAt(int x,
                                      int y,
                                      ui::mojom::MenuSourceType source_type) {
  last_source_type_for_testing_ = source_type;
  menu_runner_ = std::make_unique<views::MenuRunner>(
      split_tab_menu_.get(), views::MenuRunner::HAS_MNEMONICS,
      base::BindRepeating(&WebUISplitTabsControl::UpdateState,
                          base::Unretained(this)));

  menu_runner_->RunMenuAt(toolbar_view_->GetWidget(), nullptr,
                          gfx::Rect(gfx::Point(x, y), gfx::Size()),
                          views::MenuAnchorPosition::kTopLeft, source_type);
  UpdateState();
}

void WebUISplitTabsControl::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (selection.active_tab_changed()) {
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
    toolbar_view_->PreferredSizeChanged();
  }
}

void WebUISplitTabsControl::UpdateState() {
  auto state = toolbar_ui_api::mojom::SplitTabsControlState::New();
  auto s = webui_toolbar::ComputeTabSplitStatus(toolbar_view_->browser_);
  state->is_current_tab_split = s.is_split;
  state->location = s.location;
  state->is_pinned = webui_toolbar::IsButtonPinned(
      toolbar_view_->browser_,
      toolbar_ui_api::mojom::ToolbarButtonType::kSplitTabs);
  state->is_context_menu_visible = menu_runner_ && menu_runner_->IsRunning();
  UpdateVisibility(state.get());
  toolbar_view_->OnSplitTabsControlStateChanged(std::move(state));
}
