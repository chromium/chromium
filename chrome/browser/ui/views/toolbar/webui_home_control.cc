// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_home_control.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
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

WebUIHomeControl::WebUIHomeControl(WebUIToolbarControlDelegate* delegate)
    : delegate_(delegate), home_menu_(delegate_->GetBrowser(), kActionHome) {}

WebUIHomeControl::~WebUIHomeControl() = default;

void WebUIHomeControl::Init() {
  PrefService* prefs = delegate_->GetBrowser()->GetProfile()->GetPrefs();

  pin_state_.Init(prefs::kShowHomeButton, prefs,
                  base::BindRepeating(&WebUIHomeControl::OnIsPinnedChanged,
                                      base::Unretained(this)));

  OnIsPinnedChanged();
  UpdateState();
}

bool WebUIHomeControl::IsPinned() const {
  return is_pinned_;
}

void WebUIHomeControl::HandleContextMenu(
    const gfx::Rect& screen_rect,
    ui::mojom::MenuSourceType source_type) {
  last_source_type_for_testing_ = source_type;
  menu_runner_ = std::make_unique<views::MenuRunner>(
      &home_menu_, views::MenuRunner::HAS_MNEMONICS,
      base::BindRepeating(&WebUIHomeControl::UpdateState,
                          base::Unretained(this)));

  menu_runner_->RunMenuAt(delegate_->GetView()->GetWidget(), nullptr,
                          screen_rect, views::MenuAnchorPosition::kTopLeft,
                          source_type);

  UpdateState();
}

void WebUIHomeControl::ShowSetHomePageBubble(const GURL& undo_url,
                                             bool undo_is_ntp) {
  if (!undo_bubble_coordinator_) {
    undo_bubble_coordinator_ = std::make_unique<HomePageUndoBubbleCoordinator>(
        delegate_->GetBrowser()->GetProfile()->GetPrefs());
  }

  // Show the bubble aligned with the home button using its TrackedElement.
  ui::TrackedElement* home_button_element =
      BrowserElements::From(delegate_->GetBrowser())
          ->GetElement(kToolbarHomeButtonElementId);
  if (home_button_element) {
    undo_bubble_coordinator_->Show(undo_url, undo_is_ntp,
                                   views::BubbleAnchor(home_button_element));
  }
}

void WebUIHomeControl::OnHomeButtonDropUrl(const GURL& url) {
  PrefService* prefs = delegate_->GetBrowser()->GetProfile()->GetPrefs();
  GURL old_url = GURL(prefs->GetString(prefs::kHomePage));
  bool old_is_ntp = prefs->GetBoolean(prefs::kHomePageIsNewTabPage);

  prefs->SetString(prefs::kHomePage, url.spec());
  prefs->SetBoolean(prefs::kHomePageIsNewTabPage, false);

  ShowSetHomePageBubble(old_url, old_is_ntp);
}

void WebUIHomeControl::OnIsPinnedChanged() {
  bool old_is_pinned = is_pinned_;
  is_pinned_ = pin_state_.GetValue();
  if (is_pinned_ == old_is_pinned) {
    return;
  }
  delegate_->OnPreferredSizeChanged();
  UpdateState();
}

void WebUIHomeControl::UpdateState() {
  auto state = toolbar_ui_api::mojom::HomeControlState::New();
  state->should_be_shown = is_pinned_;
  state->is_context_menu_visible = menu_runner_ && menu_runner_->IsRunning();
  is_context_menu_visible_ = state->is_context_menu_visible;

  delegate_->OnHomeControlStateChanged(std::move(state));
}
