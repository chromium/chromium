// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_toolbar_extensions_container_wrapper.h"

#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/extensions/extensions_container.h"
#include "chrome/browser/ui/extensions/settings_api_bubble_helpers.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_model.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_extensions_container.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension_features.h"
#include "mojo/public/cpp/bindings/clone_traits.h"
#include "mojo/public/cpp/bindings/equals_traits.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/models/image_model.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

WebUIToolbarExtensionsContainerWrapper::WebUIToolbarExtensionsContainerWrapper(
    WebUIToolbarControlDelegate* delegate)
    : delegate_(delegate) {}

WebUIToolbarExtensionsContainerWrapper::
    ~WebUIToolbarExtensionsContainerWrapper() = default;

void WebUIToolbarExtensionsContainerWrapper::Init(
    content::WebContents* web_contents) {
  BrowserWindowInterface* browser = delegate_->GetBrowser();

  extensions_container_ = std::make_unique<WebUIToolbarExtensionsContainer>(
      *browser, delegate_->GetView()->GetWidget(), web_contents->GetWeakPtr(),
      &delegate_->GetIconTable(),
      /*push_icon_table_updates=*/false);

  extensions_container_->SetObserver(this);

  // Register `extensions_container_` as the `ExtensionsContainer` for
  // `browser`.
  scoped_extensions_container_user_data_ =
      std::make_unique<ui::ScopedUnownedUserData<ExtensionsContainer>>(
          browser->GetUnownedUserDataHost(), *extensions_container_);

  active_tab_subscription_ =
      browser->RegisterActiveTabDidChange(base::BindRepeating(
          &WebUIToolbarExtensionsContainerWrapper::OnActiveTabChanged,
          base::Unretained(this)));
  OnActiveTabChanged(browser);
}

void WebUIToolbarExtensionsContainerWrapper::OnThemeChanged() {
  if (extensions_container_) {
    // Icons may need re-rendering.
    extensions_container_->NotifyOfAllActions();
  }
}

bool WebUIToolbarExtensionsContainerWrapper::UpdateExtensionsButtonState() {
  ExtensionsToolbarViewModel::ExtensionsToolbarButtonState new_state =
      ExtensionsToolbarViewModel::ExtensionsToolbarButtonState::kDefault;

  if (base::FeatureList::IsEnabled(
          extensions_features::kExtensionsMenuAccessControl)) {
    // Ask ExtensionsToolbarViewModel::GetButtonState() for actual state.
    content::WebContents* web_contents =
        delegate_->GetBrowser()->GetTabStripModel()->GetActiveWebContents();
    if (web_contents && extensions_container_) {
      new_state = extensions_container_->GetToolbarViewModel()->GetButtonState(
          *web_contents);
    }
  }

  if (new_state != extensions_button_state_) {
    extensions_button_state_ = new_state;
    return true;
  }
  return false;
}

extensions_bar::mojom::ExtensionActionInfoPtr
WebUIToolbarExtensionsContainerWrapper::GetExtensionsButton() {
  extensions_bar::mojom::ExtensionActionInfoPtr button =
      extensions_bar::mojom::ExtensionActionInfo::New();

  button->id = kExtensionsButtonId;
  button->is_visible = true;
  // The extensions button is never hidden due to overflow.
  button->force_visible = true;

  // Fill in the rest of the fields based on `extensions_button_state_`.
  button->icon = extensions_button_icon_ =
      delegate_->GetIconTable().RegisterImageModelTryReuse(
          ui::ImageModel::FromVectorIcon(
              ExtensionsToolbarViewModel::GetToolbarButtonIcon(
                  extensions_button_state_)),
          extensions_button_icon_);
  button->accessible_name =
      ExtensionsToolbarViewModel::GetToolbarButtonAccessibleText(
          extensions_button_state_);
  button->tooltip = ExtensionsToolbarViewModel::GetToolbarButtonTooltipText(
      extensions_button_state_);

  return button;
}

void WebUIToolbarExtensionsContainerWrapper::OnActiveTabChanged(
    BrowserWindowInterface* browser_interface) {
  if (browser_interface && browser_interface->GetTabStripModel()) {
    content::WebContents* active_contents =
        browser_interface->GetTabStripModel()->GetActiveWebContents();
    if (active_contents) {
      // No need to check whether the active tab is an extension-controlled NTP
      // here; the helper inspects the tab's visible navigation entry and
      // returns early unless it is an extension URL matching the overridden
      // NTP. This mirrors ExtensionsToolbarDesktop::OnActiveWebContentsChanged.
      extensions::MaybeShowExtensionControlledNewTabPage(browser_interface,
                                                         active_contents);
    }
  }
}

void WebUIToolbarExtensionsContainerWrapper::OnActionsAddedOrUpdated(
    std::vector<toolbar_ui_api::mojom::IconUpdatePtr> icons,
    std::vector<extensions_bar::mojom::ExtensionActionInfoPtr> actions) {
  bool changed = false;

  std::vector<std::string> new_ids;
  for (const auto& action : actions) {
    if (action->is_visible) {
      new_ids.push_back(action->id);
    }
  }
  if (new_ids != last_sent_extension_ids_) {
    changed = true;
  }

  for (auto& action : actions) {
    auto it = cached_actions_.find(action->id);
    if (it == cached_actions_.end() || !mojo::Equals(it->second, action)) {
      cached_actions_[action->id] = std::move(action);
      changed = true;
    }
  }

  const bool button_changed = UpdateExtensionsButtonState();
  if (changed || button_changed) {
    SendExtensionsState();
  }
}

void WebUIToolbarExtensionsContainerWrapper::OnActionRemoved(
    std::vector<toolbar_ui_api::mojom::IconUpdatePtr> icons,
    const std::string& id) {
  const bool action_removed = cached_actions_.erase(id) > 0;
  const bool button_changed = UpdateExtensionsButtonState();
  if (action_removed || button_changed) {
    SendExtensionsState();
  }
}

void WebUIToolbarExtensionsContainerWrapper::OnActionPoppedOut(
    const std::string& action_id,
    base::OnceClosure callback) {
  if (!extensions_container_) {
    std::move(callback).Run();
    return;
  }

  // The container cancels any pending pop out request before starting another,
  // so there should be at most one at a time.
  CHECK(!pending_pop_out_action_id_);
  CHECK(!pending_pop_out_callback_);

  // Wait for the popped out action's button to be registered. The button waits
  // for any animations it's running to complete before registering with the
  // TrackedElementManager, so its location shouldn't shift after `callback` is
  // run (and potentially upset any pop-up anchoring that
  // ExtensionActionDelegateDesktop::ShowPopup() or any other caller might want
  // to display).
  //
  // Also wait for the extensions menu button, which is always shown when there
  // are any extensions. When the first extension is added, both buttons are
  // animated in, and since the renderer reports each element's visibility
  // separately, the popped out action's button may be reported before the
  // menu button has finished animating, which could still affect the popped
  // out button's location.
  //
  // If the action is the pinned-by-default IPH anchor (which only happens for
  // IPH requests, since PopOutAction() clears the IPH anchor first), also wait
  // for kExtensionsPinnedByDefaultElementId, which the IPH is anchored to. That
  // element is registered without waiting for animations, so waiting on it
  // alone doesn't guarantee the button is in its final location.
  std::optional<ui::ElementIdentifier> missing_element_id;
  if (!extensions_container_->GetExtensionAnchor(action_id)) {
    missing_element_id =
        WebUIToolbarExtensionsContainer::GetElementId(action_id);
  } else if (!extensions_container_->GetExtensionAnchor(kExtensionsButtonId)) {
    missing_element_id =
        WebUIToolbarExtensionsContainer::GetElementId(kExtensionsButtonId);
  } else if (auto it = cached_actions_.find(action_id);
             it != cached_actions_.end() &&
             it->second->is_pinned_by_default_iph_anchor &&
             !extensions_container_->GetExtensionElement(
                 kExtensionsPinnedByDefaultElementId, action_id)) {
    missing_element_id = kExtensionsPinnedByDefaultElementId;
  }

  if (!missing_element_id) {
    std::move(callback).Run();
    return;
  }

  pending_pop_out_action_id_ = action_id;
  pending_pop_out_callback_ = std::move(callback);
  pending_pop_out_subscription_ =
      ui::ElementTracker::GetElementTracker()->AddElementShownCallback(
          *missing_element_id,
          BrowserElements::From(delegate_->GetBrowser())->GetContext(),
          base::BindRepeating(
              &WebUIToolbarExtensionsContainerWrapper::OnElementShown,
              base::Unretained(this)));
}

void WebUIToolbarExtensionsContainerWrapper::OnPopOutCancelled() {
  pending_pop_out_subscription_ = {};
  pending_pop_out_action_id_.reset();
  if (pending_pop_out_callback_) {
    std::exchange(pending_pop_out_callback_, base::OnceClosure()).Run();
  }
}

void WebUIToolbarExtensionsContainerWrapper::OnElementShown(
    ui::TrackedElement* element) {
  // Re-dispatch the pending pop out request, which will either run it or wait
  // for another element to be shown. Since all extension buttons share an
  // ElementIdentifier, the shown element may belong to another action, in which
  // case OnActionPoppedOut() will resume waiting.
  CHECK(pending_pop_out_action_id_);
  CHECK(pending_pop_out_callback_);
  pending_pop_out_subscription_ = {};
  OnActionPoppedOut(
      std::exchange(pending_pop_out_action_id_, std::nullopt).value(),
      std::exchange(pending_pop_out_callback_, base::OnceClosure()));
}

void WebUIToolbarExtensionsContainerWrapper::SendExtensionsState() {
  std::vector<extensions_bar::mojom::ExtensionActionInfoPtr> state;
  std::vector<std::string> sent_ids;
  if (extensions_container_) {
    for (const auto& id : extensions_container_->GetOrderedActionIds()) {
      auto it = cached_actions_.find(id);
      if (it != cached_actions_.end()) {
        if (it->second->is_visible) {
          state.push_back(mojo::Clone(it->second));
          sent_ids.push_back(id);
        }
      }
    }
  }
  if (!cached_actions_.empty()) {
    state.push_back(GetExtensionsButton());
  }
  last_sent_extension_ids_ = std::move(sent_ids);
  delegate_->OnExtensionsStateChanged(std::move(state));
}

void WebUIToolbarExtensionsContainerWrapper::ExecuteUserAction(
    const std::string& extension_id) {
  if (extensions_container_) {
    if (extension_id == kExtensionsButtonId) {
      extensions_container_->ToggleExtensionsMenu();
    } else {
      extensions_container_->ExecuteUserAction(extension_id);
    }
  }
}

void WebUIToolbarExtensionsContainerWrapper::ShowContextMenu(
    ui::mojom::MenuSourceType source,
    const std::string& extension_id) {
  if (extensions_container_ && extension_id != kExtensionsButtonId) {
    extensions_container_->ShowContextMenu(source, extension_id);
  }
}

void WebUIToolbarExtensionsContainerWrapper::MoveExtension(
    const std::string& extension_id,
    int32_t target_index) {
  if (extensions_container_) {
    extensions_container_->MoveExtensionAction(extension_id, target_index);
  }
}

void WebUIToolbarExtensionsContainerWrapper::MoveExtensionBy(
    const std::string& extension_id,
    int32_t delta) {
  if (extensions_container_) {
    extensions_container_->MoveExtensionActionBy(extension_id, delta);
  }
}
