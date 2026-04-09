// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_home_control.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/browser/ui/webui/webui_toolbar/utils/toolbar_button_utils.h"
#include "chrome/common/pref_names.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "components/prefs/pref_service.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/widget/widget.h"

WebUIHomeControl::WebUIHomeControl(WebUIToolbarWebView* webui_toolbar_web_view)
    : webui_toolbar_web_view_(webui_toolbar_web_view),
      home_menu_(webui_toolbar_web_view_->browser_, kActionHome) {}

WebUIHomeControl::~WebUIHomeControl() = default;

void WebUIHomeControl::Init() {
  PrefService* prefs =
      webui_toolbar_web_view_->browser_->GetProfile()->GetPrefs();

  pin_state_.Init(prefs::kShowHomeButton, prefs,
                  base::BindRepeating(&WebUIHomeControl::UpdateState,
                                      base::Unretained(this)));

  UpdateState();
}

bool WebUIHomeControl::IsVisible() const {
  return is_visible_;
}

void WebUIHomeControl::HandleContextMenu(
    const gfx::Rect& screen_rect,
    ui::mojom::MenuSourceType source_type) {
  last_source_type_for_testing_ = source_type;
  menu_runner_ = std::make_unique<views::MenuRunner>(
      &home_menu_, views::MenuRunner::HAS_MNEMONICS,
      base::BindRepeating(&WebUIHomeControl::UpdateState,
                          base::Unretained(this)));

  menu_runner_->RunMenuAt(webui_toolbar_web_view_->GetWidget(), nullptr,
                          screen_rect, views::MenuAnchorPosition::kTopLeft,
                          source_type);
  UpdateState();
}

void WebUIHomeControl::ShowSetHomePageBubble(const GURL& undo_url,
                                             bool undo_is_ntp) {
  if (!undo_bubble_coordinator_) {
    undo_bubble_coordinator_ = std::make_unique<HomePageUndoBubbleCoordinator>(
        webui_toolbar_web_view_->browser_->GetProfile()->GetPrefs());
  }

  // Show the bubble aligned with the home button using its TrackedElement.
  ui::TrackedElement* home_button_element =
      BrowserElements::From(webui_toolbar_web_view_->browser_)
          ->GetElement(kToolbarHomeButtonElementId);
  if (home_button_element) {
    undo_bubble_coordinator_->Show(undo_url, undo_is_ntp,
                                   views::BubbleAnchor(home_button_element));
  }
}

void WebUIHomeControl::OnHomeButtonDropUrl(const GURL& url) {
  PrefService* prefs =
      webui_toolbar_web_view_->browser_->GetProfile()->GetPrefs();
  GURL old_url = GURL(prefs->GetString(prefs::kHomePage));
  bool old_is_ntp = prefs->GetBoolean(prefs::kHomePageIsNewTabPage);

  prefs->SetString(prefs::kHomePage, url.spec());
  prefs->SetBoolean(prefs::kHomePageIsNewTabPage, false);

  ShowSetHomePageBubble(old_url, old_is_ntp);
}

void WebUIHomeControl::UpdateVisibility(
    const toolbar_ui_api::mojom::HomeControlState* state) {
  bool should_be_visible = state->is_pinned;

  if (should_be_visible != is_visible_) {
    is_visible_ = should_be_visible;
    webui_toolbar_web_view_->PreferredSizeChanged();
  }
}

void WebUIHomeControl::UpdateState() {
  auto state = toolbar_ui_api::mojom::HomeControlState::New();
  state->is_pinned = webui_toolbar::IsButtonPinned(
      webui_toolbar_web_view_->browser_,
      toolbar_ui_api::mojom::ToolbarButtonType::kHome);
  state->is_context_menu_visible = menu_runner_ && menu_runner_->IsRunning();
  UpdateVisibility(state.get());
  webui_toolbar_web_view_->OnHomeControlStateChanged(std::move(state));
}
