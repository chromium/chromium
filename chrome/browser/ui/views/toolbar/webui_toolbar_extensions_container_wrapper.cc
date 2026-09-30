// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_toolbar_extensions_container_wrapper.h"

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/extensions/extensions_container.h"
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

struct WebUIToolbarExtensionsContainerWrapper::PendingAnchorRequest {
  PendingAnchorRequest(base::OnceClosure cb,
                       ui::ElementTracker::Subscription sub)
      : callback(std::move(cb)), subscription(std::move(sub)) {}
  ~PendingAnchorRequest() = default;
  base::OnceClosure callback;
  const ui::ElementTracker::Subscription subscription;
};

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
    Profile* profile = delegate_->GetBrowser()->GetProfile();
    if (web_contents && extensions_container_ && profile) {
      new_state = ExtensionsToolbarViewModel::GetButtonState(
          delegate_->GetBrowser(), *web_contents,
          ToolbarActionsModel::Get(profile),
          base::BindOnce(&WebUIToolbarExtensionsContainerWrapper::
                             AnyActionHasCurrentSiteAccess,
                         base::Unretained(this)));
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

bool WebUIToolbarExtensionsContainerWrapper::AnyActionHasCurrentSiteAccess(
    content::WebContents& web_contents) {
  for (const auto& [_, action] : cached_actions_) {
    ToolbarActionViewModel* action_model =
        extensions_container_->GetActionForId(action->id);
    if (action_model &&
        action_model->GetSiteInteraction(&web_contents) ==
            extensions::SitePermissionsHelper::SiteInteraction::kGranted) {
      return true;
    }
  }
  return false;
}

void WebUIToolbarExtensionsContainerWrapper::OnActiveTabChanged(
    BrowserWindowInterface* browser_interface) {
  if (browser_interface && browser_interface->GetTabStripModel()) {
    content::WebContentsObserver::Observe(
        browser_interface->GetTabStripModel()->GetActiveWebContents());
  }
  if (extensions_container_) {
    // State of extensions depends on what's active --- e.g. some may be
    // disabled on some URLs.
    extensions_container_->NotifyOfAllActions();
  }
}

void WebUIToolbarExtensionsContainerWrapper::PrimaryPageChanged(
    content::Page& page) {
  if (extensions_container_) {
    extensions_container_->NotifyOfAllActions();
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
  if (changed || UpdateExtensionsButtonState()) {
    SendExtensionsState();
  }
}

void WebUIToolbarExtensionsContainerWrapper::OnActionRemoved(
    std::vector<toolbar_ui_api::mojom::IconUpdatePtr> icons,
    const std::string& id) {
  if (cached_actions_.erase(id) > 0 || UpdateExtensionsButtonState()) {
    SendExtensionsState();
  }
}

void WebUIToolbarExtensionsContainerWrapper::OnActionPoppedOut(
    base::OnceClosure callback) {
  if (!extensions_container_) {
    std::move(callback).Run();
    return;
  }

  // Be as conservative as possible and wait for any animations that might be in
  // progress to complete before running `callback`, that way none of the
  // button locations might shift after `callback` is run (and potentially
  // upset any pop-up anchoring that ExtensionActionDelegateDesktop::ShowPopup()
  // or any other caller might want to display).
  for (const auto& action : delegate_->GetState().extensions_state) {
    if (!extensions_container_->GetExtensionAnchor(action->id)) {
      auto subscription =
          ui::ElementTracker::GetElementTracker()->AddElementShownCallback(
              WebUIToolbarExtensionsContainer::GetElementId(action->id),
              BrowserElements::From(delegate_->GetBrowser())->GetContext(),
              base::BindRepeating(
                  &WebUIToolbarExtensionsContainerWrapper::OnElementShown,
                  base::Unretained(this)));
      pending_anchor_requests_.emplace_back(std::move(callback),
                                            std::move(subscription));
      return;
    }
  }

  std::move(callback).Run();
}

void WebUIToolbarExtensionsContainerWrapper::OnElementShown(
    ui::TrackedElement* element) {
  std::vector<base::OnceClosure> callbacks_to_run;
  for (auto& request : pending_anchor_requests_) {
    callbacks_to_run.push_back(std::move(request.callback));
  }
  pending_anchor_requests_.clear();

  for (auto& callback : callbacks_to_run) {
    if (callback) {
      OnActionPoppedOut(std::move(callback));
    }
  }
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
