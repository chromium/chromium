// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_glic_control.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/notimplemented.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/glic/browser_ui/glic_split_button_controller.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "components/prefs/pref_service.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

WebUIGlicControl::WebUIGlicControl(WebUIToolbarControlDelegate* delegate)
    : delegate_(delegate) {}

WebUIGlicControl::~WebUIGlicControl() = default;

void WebUIGlicControl::Init() {
  profile_ = delegate_->GetBrowser()->GetProfile();
  PrefService* prefs = profile_->GetPrefs();
  pref_registrar_.Init(prefs);
  pref_registrar_.Add(glic::prefs::kGlicPinnedToTabstrip,
                      base::BindRepeating(&WebUIGlicControl::OnPrefChanged,
                                          base::Unretained(this)));
  is_pinned_ = prefs->GetBoolean(glic::prefs::kGlicPinnedToTabstrip);

  // Visibility (`should_show_`) is controlled by
  // `ToolbarView::UpdateGlicButtonVisibility()` via `SetVisible()`, which
  // evaluates both `GlicButtonController` state and whether the Glic button
  // belongs in the toolbar (Vertical Tabs or
  // `kGlicHorizontalTabToolbarButton`).
  if (auto* glic_service = glic::GlicKeyedService::Get(profile_)) {
    is_panel_open_ =
        glic_service->IsPanelShowingForBrowser(*delegate_->GetBrowser());
  }

  is_initialized_ = true;
  UpdateState();
}

void WebUIGlicControl::OnClicked() {
  if (auto* controller =
          glic::GlicSplitButtonController::From(delegate_->GetBrowser())) {
    controller->OnGlicButtonClicked();
  } else if (auto* glic_service = glic::GlicKeyedService::Get(profile_)) {
    glic_service->ToggleUI(delegate_->GetBrowser(), /*prevent_close=*/false,
                           glic::mojom::InvocationSource::kToolbarButton);
  }
  SetIsShowingNudge(false);
}

void WebUIGlicControl::HandleContextMenu(const gfx::Rect& screen_rect,
                                         ui::mojom::MenuSourceType source) {
  // Like the native `views::GlicButton`, the Glic button is hidden from the
  // toolbar when unpinned and does not move into the overflow menu, so this
  // context menu can only be invoked while pinned and only needs to offer
  // unpinning (re-pinning is handled via Chrome Settings).
  if (!is_pinned_) {
    return;
  }

  menu_runner_.reset();
  menu_model_adapter_.reset();
  menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  menu_model_->AddItemWithStringIdAndIcon(
      IDC_GLIC_TOGGLE_PIN, IDS_GLIC_BUTTON_CXMENU_UNPIN,
      ui::ImageModel::FromVectorIcon(
          features::IsRoundedIconsEnabled() ? kKeepOffIcon : kKeepOffOldIcon,
          ui::kColorIcon, 16));

  menu_model_adapter_ = std::make_unique<views::MenuModelAdapter>(
      menu_model_.get(), base::BindRepeating(&WebUIGlicControl::OnMenuClosed,
                                             base::Unretained(this)));
  menu_model_adapter_->set_triggerable_event_flags(ui::EF_LEFT_MOUSE_BUTTON |
                                                   ui::EF_RIGHT_MOUSE_BUTTON);

  std::unique_ptr<views::MenuItemView> root = menu_model_adapter_->CreateMenu();
  menu_runner_ = std::make_unique<views::MenuRunner>(
      std::move(root),
      views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU);

  menu_runner_->RunMenuAt(delegate_->GetView()->GetWidget(), nullptr,
                          screen_rect, views::MenuAnchorPosition::kTopLeft,
                          source);
  UpdateState();
}

void WebUIGlicControl::SetIsShowingNudge(bool is_showing) {
  if (is_showing_nudge_ == is_showing) {
    return;
  }
  is_showing_nudge_ = is_showing;
  if (!is_showing) {
    nudge_label_.clear();
  }
  UpdateState();
}

bool WebUIGlicControl::GetIsShowingNudge() const {
  return is_showing_nudge_;
}

void WebUIGlicControl::SetNudgeLabel(std::string label) {
  if (nudge_label_ == label) {
    return;
  }
  nudge_label_ = std::move(label);
  if (is_showing_nudge_) {
    UpdateState();
  }
}

void WebUIGlicControl::SetVisible(bool visible) {
  if (should_show_ == visible) {
    return;
  }
  should_show_ = visible;
  if (!IsVisible()) {
    SetIsShowingNudge(false);
  }
  UpdateState();
  delegate_->OnPreferredSizeChanged();
}

void WebUIGlicControl::SetGlicPanelIsOpen(bool open) {
  if (is_panel_open_ == open) {
    return;
  }
  is_panel_open_ = open;
  UpdateState();
}

void WebUIGlicControl::UpdateStyle(bool should_match_toolbar) {
  // TODO(crbug.com/510825665): Support non-matching background style in WebUI.
  NOTIMPLEMENTED();
}

bool WebUIGlicControl::IsContextMenuShowingForTesting() const {
  return menu_runner_ && menu_runner_->IsRunning();
}

void WebUIGlicControl::ExecuteCommand(int command_id, int event_flags) {
  if (command_id == IDC_GLIC_TOGGLE_PIN) {
    profile_->GetPrefs()->SetBoolean(glic::prefs::kGlicPinnedToTabstrip, false);
  }
}

void WebUIGlicControl::OnMenuClosed() {
  menu_runner_.reset();
  menu_model_adapter_.reset();
  menu_model_.reset();
  UpdateState();
}

void WebUIGlicControl::OnPrefChanged() {
  bool is_pinned =
      profile_->GetPrefs()->GetBoolean(glic::prefs::kGlicPinnedToTabstrip);
  if (is_pinned_ == is_pinned) {
    return;
  }
  is_pinned_ = is_pinned;
  if (!is_pinned_) {
    SetIsShowingNudge(false);
    if (menu_runner_) {
      menu_runner_.reset();
      menu_model_adapter_.reset();
      menu_model_.reset();
    }
  }
  UpdateState();
  delegate_->OnPreferredSizeChanged();
}

void WebUIGlicControl::UpdateState() {
  if (!is_initialized_) {
    return;
  }
  auto state = toolbar_ui_api::mojom::GlicButtonState::New();
  state->open = is_panel_open_;
  state->should_show = IsVisible();
  state->is_context_menu_visible = menu_runner_ && menu_runner_->IsRunning();
  if (is_showing_nudge_ && !nudge_label_.empty()) {
    state->nudge_label = nudge_label_;
  }
  delegate_->OnGlicButtonStateChanged(std::move(state));
}
