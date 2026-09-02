// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_overflow_button.h"

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "base/check.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/side_panel/side_panel_action_callback.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/views/toolbar/toolbar_controller.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api.mojom.h"
#include "mojo/public/mojom/base/error.mojom.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "ui/actions/action_id.h"
#include "ui/actions/actions.h"

namespace {

// Returns true if specified element can be hidden due to overflow and show on
// the overflow menu.
bool ElementCanOverflow(ui::ElementIdentifier id) {
  return (id == kToolbarForwardButtonElementId ||
          id == kToolbarHomeButtonElementId ||
          id == kToolbarSplitTabsToolbarButtonElementId);
}

}  // namespace

WebUIOverflowButton::WebUIOverflowButton(
    WebUIToolbarControlDelegate* delegate,
    OverflowMenu::PinnedActionsInfo* pinned_actions_info)
    : delegate_(delegate), pinned_actions_info_(pinned_actions_info) {}

WebUIOverflowButton::~WebUIOverflowButton() = default;

void WebUIOverflowButton::ExecuteCommand(
    const OverflowMenu::OverflowableElement& element) {
  std::visit(
      absl::Overload{[this](actions::ActionId id) {
                       pinned_actions_info_->GetActionItemFor(id)->InvokeAction(
                           actions::ActionInvocationContext::Builder()
                               .SetProperty(kSidePanelOpenTriggerKey,
                                            SidePanelOpenTrigger::kOverflowMenu)
                               .Build());
                     },
                     [this](OverflowMenu::ElementIdInfo element_info) {
                       delegate_->OverflowButtonClicked(
                           element_info.overflow_identifier);
                     }},
      element);

  std::string action_name =
      ToolbarController::GetActionNameFromElementIdentifier(
          OverflowableElementInfoToId(element));
  if (!action_name.empty()) {
    base::RecordAction(
        base::UserMetricsAction("ResponsiveToolbar.OverflowMenuItemActivated"));
    base::RecordAction(base::UserMetricsAction(action_name.c_str()));
  }
}

bool WebUIOverflowButton::IsCurrentlyOverflowed(
    const OverflowMenu::OverflowableElement& element) const {
  return overflowed_elements_.find(OverflowableElementInfoToId(element)) !=
         overflowed_elements_.end();
}

bool WebUIOverflowButton::IsEnabled(
    const OverflowMenu::OverflowableElement& element) const {
  auto it = overflowed_elements_.find(OverflowableElementInfoToId(element));
  // This method should only be called by the overflow menu for items we told it
  // to list, so this CHECK() should be safe.
  CHECK(it != overflowed_elements_.end());
  return it->second.is_enabled;
}

void WebUIOverflowButton::OnMenuClosed() {
  overflow_menu_.reset();
  overflowed_elements_.clear();
  UpdateState();
}

void WebUIOverflowButton::UpdateState() {
  auto state = toolbar_ui_api::mojom::OverflowButtonControlState::New();
  state->is_context_menu_visible = overflow_menu_ != nullptr;
  delegate_->OnOverflowButtonControlStateChanged(std::move(state));
}

void WebUIOverflowButton::ShowOverflowMenu(
    const std::vector<toolbar_ui_api::mojom::OverflowMenuItemPtr>& controls,
    const gfx::Rect& screen_rect,
    ui::mojom::MenuSourceType source,
    toolbar_ui_api::mojom::ToolbarUIService::ShowOverflowMenuCallback
        callback) {
  BrowserWindowInterface* browser = delegate_->GetBrowser();

  std::map<OverflowableElementId, OverflowedElementInfo>
      new_overflowed_elements;
  for (const auto& item : controls) {
    auto element_id =
        ui::ElementIdentifier::FromName(item->id->native_identifier.c_str());
    if (!element_id || !ElementCanOverflow(element_id)) {
      std::move(callback).Run(base::unexpected(mojo_base::mojom::Error::New(
          mojo_base::mojom::Code::kInvalidArgument,
          base::StringPrintf("WebUIOverflowButton: Unknown control ID: %s",
                             item->id->native_identifier.c_str()))));
      return;
    }
    new_overflowed_elements.emplace(
        element_id, OverflowedElementInfo{.is_enabled = item->is_enabled});
  }

  // Destroy old overflow menu, if there is one.
  if (overflow_menu_) {
    OnMenuClosed();
  }
  overflowed_elements_ = std::move(new_overflowed_elements);

  // If there are no overflowed elements, do nothing. It's unclear if this can
  // happen when the Javascript code is behaving as intended - it depends on the
  // specifics of Javascript mouse ordering and task execution order/Mojo task
  // preemption.
  if (overflowed_elements_.empty()) {
    std::move(callback).Run(std::monostate());
    return;
  }

  overflow_menu_ = std::make_unique<OverflowMenu>(
      OverflowMenu::GetDefaultResponsiveElements(browser), this,
      pinned_actions_info_,
      PinnedToolbarActionsModel::Get(browser->GetProfile()));

  overflow_menu_->ShowMenu(delegate_->GetView()->GetWidget(), nullptr,
                           screen_rect);
  UpdateState();
  std::move(callback).Run(std::monostate());
}

WebUIOverflowButton::OverflowableElementId
WebUIOverflowButton::OverflowableElementInfoToId(
    const OverflowMenu::OverflowableElement& element) {
  return std::visit(
      absl::Overload{
          [](actions::ActionId id) -> OverflowableElementId { return id; },
          [](const OverflowMenu::ElementIdInfo& info) -> OverflowableElementId {
            return info.overflow_identifier;
          }},
      element);
}
