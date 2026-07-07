// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_toolbar_extensions_container_wrapper.h"

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/extensions/extensions_container.h"
#include "chrome/browser/ui/extensions/extensions_toolbar_view_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_model.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_extensions_container.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension_features.h"
#include "mojo/public/cpp/bindings/clone_traits.h"
#include "mojo/public/cpp/bindings/equals_traits.h"
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
}

void WebUIToolbarExtensionsContainerWrapper::OnThemeChanged() {
  if (extensions_container_) {
    // Icons may need re-rendering.
    extensions_container_->NotifyOfAllActions();
  }
}

extensions_bar::mojom::ExtensionActionInfoPtr
WebUIToolbarExtensionsContainerWrapper::GetExtensionsButton() {
  ExtensionsToolbarViewModel::ExtensionsToolbarButtonState state =
      ExtensionsToolbarViewModel::ExtensionsToolbarButtonState::kDefault;

  if (base::FeatureList::IsEnabled(
          extensions_features::kExtensionsMenuAccessControl)) {
    // Ask ExtensionsToolbarViewModel::GetButtonState() for actual state.
    content::WebContents* web_contents =
        delegate_->GetBrowser()->GetTabStripModel()->GetActiveWebContents();
    Profile* profile = delegate_->GetBrowser()->GetProfile();
    if (web_contents && extensions_container_ && profile) {
      state = ExtensionsToolbarViewModel::GetButtonState(
          delegate_->GetBrowser(), *web_contents,
          ToolbarActionsModel::Get(profile),
          base::BindOnce(&WebUIToolbarExtensionsContainerWrapper::
                             AnyActionHasCurrentSiteAccess,
                         base::Unretained(this)));
    }
  }

  extensions_bar::mojom::ExtensionActionInfoPtr button =
      extensions_bar::mojom::ExtensionActionInfo::New();

  button->id = kExtensionsButtonId;
  button->is_visible = true;

  // Fill in the rest of the fields based on `state`.
  auto icon_handle = delegate_->GetIconTable().RegisterVectorIcon(
      ExtensionsToolbarViewModel::GetToolbarButtonIcon(state));
  CHECK(icon_handle.has_value());
  button->icon = icon_handle.value();
  button->accessible_name =
      ExtensionsToolbarViewModel::GetToolbarButtonAccessibleText(state);
  button->tooltip =
      ExtensionsToolbarViewModel::GetToolbarButtonTooltipText(state);

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
  if (extensions_container_) {
    // State of extensions depends on what's active --- e.g. some may be
    // disabled on some URLs.
    extensions_container_->NotifyOfAllActions();
  }
}

void WebUIToolbarExtensionsContainerWrapper::OnActionsAddedOrUpdated(
    std::vector<toolbar_ui_api::mojom::IconUpdatePtr> icons,
    std::vector<extensions_bar::mojom::ExtensionActionInfoPtr> actions) {
  bool changed = false;
  for (auto& action : actions) {
    auto it = cached_actions_.find(action->id);
    if (it == cached_actions_.end() || !mojo::Equals(it->second, action)) {
      cached_actions_[action->id] = std::move(action);
      changed = true;
    }
  }
  if (changed) {
    SendExtensionsState();
  }
}

void WebUIToolbarExtensionsContainerWrapper::OnActionRemoved(
    std::vector<toolbar_ui_api::mojom::IconUpdatePtr> icons,
    const std::string& id) {
  if (cached_actions_.erase(id) > 0) {
    SendExtensionsState();
  }
}

void WebUIToolbarExtensionsContainerWrapper::OnActionPoppedOut(
    base::OnceClosure callback) {
  // TODO: Need to delay here until the WebUI animates out the icon and a
  // TrackedElement is available to anchor to.
  std::move(callback).Run();
}

void WebUIToolbarExtensionsContainerWrapper::SendExtensionsState() {
  std::vector<extensions_bar::mojom::ExtensionActionInfoPtr> state;
  for (const auto& [id, action] : cached_actions_) {
    if (!action->is_visible) {
      continue;
    }
    state.push_back(mojo::Clone(action));
  }
  if (!cached_actions_.empty()) {
    state.push_back(GetExtensionsButton());
  }
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
