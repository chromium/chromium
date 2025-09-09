// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui_browser/webui_browser_extensions_container.h"

#include <optional>

#include "base/logging.h"
#include "base/notimplemented.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/extension_action_view_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_delegate.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar_bubble_delegate.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/webui/util/image_util.h"
#include "chrome/browser/ui/webui_browser/webui_browser_ui.h"
#include "chrome/browser/ui/webui_browser/webui_browser_window.h"

class WebUIBrowserExtensionsContainer::ActionInfo
    : public ToolbarActionViewDelegate {
 public:
  ActionInfo(WebUIBrowserExtensionsContainer& extensions_container,
             Browser& browser,
             std::unique_ptr<ToolbarActionViewController> controller)
      : extensions_container_(extensions_container),
        browser_(browser),
        controller_(std::move(controller)) {
    controller_->SetDelegate(this);
  }

  // ToolbarActionViewDelegate:
  content::WebContents* GetCurrentWebContents() const override {
    return browser_->tab_strip_model()->GetActiveWebContents();
  }

  void UpdateState() override {
    extensions_container_->NotifyOfOneAction(controller_->GetId());
  }

  void ShowContextMenuAsFallback() override { NOTIMPLEMENTED(); }

  ToolbarActionViewController* controller() { return controller_.get(); }

  extensions_bar::mojom::ExtensionActionInfoPtr ToMojo(
      WebUIBrowserWindow& window,
      const ToolbarActionsModel& tab_model,
      const ui::ColorProvider* color_provider) const {
    content::WebContents* web_contents = GetCurrentWebContents();
    auto result = extensions_bar::mojom::ExtensionActionInfo::New();
    result->id = controller_->GetId();
    result->accessible_name =
        base::UTF16ToUTF8(controller_->GetAccessibleName(web_contents));
    result->is_pinned = tab_model.IsActionPinned(result->id);
    result->is_enabled = controller_->IsEnabled(web_contents);

    ui::ImageModel icon_model =
        controller_->GetIcon(web_contents, gfx::Size(20, 20));
    if (cached_icon_model_ != icon_model || !cached_icon_url_.has_value()) {
      cached_icon_model_ = std::move(icon_model);
      cached_icon_url_ = GURL(webui::EncodePNGAndMakeDataURI(
          cached_icon_model_->Rasterize(color_provider),
          window.GetWebUIBrowserUI()->web_ui()->GetDeviceScaleFactor()));
    }
    result->data_url_for_icon = *cached_icon_url_;
    return result;
  }

 private:
  const raw_ref<WebUIBrowserExtensionsContainer> extensions_container_;
  const raw_ref<Browser> browser_;
  mutable std::optional<ui::ImageModel> cached_icon_model_;
  mutable std::optional<GURL> cached_icon_url_;
  std::unique_ptr<ToolbarActionViewController> controller_;
};

WebUIBrowserExtensionsContainer::WebUIBrowserExtensionsContainer(
    Browser& browser,
    WebUIBrowserWindow& window)
    : browser_(browser),
      window_(window),
      model_(*ToolbarActionsModel::Get(browser.profile())) {
  CreateActions();
  observe_actions_.Observe(&model_.get());
}

WebUIBrowserExtensionsContainer::~WebUIBrowserExtensionsContainer() = default;

ToolbarActionViewController* WebUIBrowserExtensionsContainer::GetActionForId(
    const std::string& action_id) {
  auto it = actions_.find(action_id);
  return it != actions_.end() ? it->second->controller() : nullptr;
}

std::optional<extensions::ExtensionId>
WebUIBrowserExtensionsContainer::GetPoppedOutActionId() const {
  NOTIMPLEMENTED();
  return std::nullopt;
}

void WebUIBrowserExtensionsContainer::OnContextMenuShownFromToolbar(
    const std::string& action_id) {
  NOTIMPLEMENTED();
}

void WebUIBrowserExtensionsContainer::OnContextMenuClosedFromToolbar() {
  NOTIMPLEMENTED();
}

bool WebUIBrowserExtensionsContainer::IsActionVisibleOnToolbar(
    const std::string& action_id) const {
  NOTIMPLEMENTED();
  return false;
}

void WebUIBrowserExtensionsContainer::UndoPopOut() {
  NOTIMPLEMENTED();
}

void WebUIBrowserExtensionsContainer::SetPopupOwner(
    ToolbarActionViewController* popup_owner) {
  NOTIMPLEMENTED();
}

void WebUIBrowserExtensionsContainer::HideActivePopup() {
  NOTIMPLEMENTED();
}

bool WebUIBrowserExtensionsContainer::CloseOverflowMenuIfOpen() {
  NOTIMPLEMENTED();
  return true;
}

void WebUIBrowserExtensionsContainer::PopOutAction(
    const extensions::ExtensionId& action_id,
    base::OnceClosure closure) {
  NOTIMPLEMENTED();
}

bool WebUIBrowserExtensionsContainer::ShowToolbarActionPopupForAPICall(
    const std::string& action_id,
    ShowPopupCallback callback) {
  NOTIMPLEMENTED();
  return true;
}

void WebUIBrowserExtensionsContainer::ShowToolbarActionBubble(
    std::unique_ptr<ToolbarActionsBarBubbleDelegate> bubble) {
  NOTIMPLEMENTED();
}

void WebUIBrowserExtensionsContainer::ToggleExtensionsMenu() {
  NOTIMPLEMENTED();
}

bool WebUIBrowserExtensionsContainer::HasAnyExtensions() const {
  NOTIMPLEMENTED();
  return true;
}

void WebUIBrowserExtensionsContainer::UpdateToolbarActionHoverCard(
    ToolbarActionView* action_view,
    ToolbarActionHoverCardUpdateType update_type) {
  NOTIMPLEMENTED();
}

void WebUIBrowserExtensionsContainer::CollapseConfirmation() {
  NOTIMPLEMENTED();
}

extensions_bar::mojom::ExtensionActionInfoPtr
WebUIBrowserExtensionsContainer::ToMojom(const ActionInfo& action_info) {
  return action_info.ToMojo(*window_, model_.get(),
                            window_->GetColorProvider());
}

void WebUIBrowserExtensionsContainer::OnToolbarModelInitialized() {
  CreateActions();
}

void WebUIBrowserExtensionsContainer::OnToolbarActionAdded(
    const ToolbarActionsModel::ActionId& id) {
  CreateActionForId(id);
  NotifyOfOneAction(id);
}

void WebUIBrowserExtensionsContainer::OnToolbarActionRemoved(
    const ToolbarActionsModel::ActionId& id) {
  actions_.erase(id);
  if (page_) {
    page_->ActionRemoved(id);
  }
}

void WebUIBrowserExtensionsContainer::OnToolbarActionUpdated(
    const ToolbarActionsModel::ActionId& id) {
  NotifyOfOneAction(id);
}

void WebUIBrowserExtensionsContainer::OnToolbarPinnedActionsChanged() {
  NotifyOfAllActions();
}

void WebUIBrowserExtensionsContainer::Bind(
    mojo::PendingRemote<extensions_bar::mojom::Page> page,
    mojo::PendingReceiver<extensions_bar::mojom::PageHandler> receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
  page_.reset();
  page_.Bind(std::move(page));
  NotifyOfAllActions();
}

void WebUIBrowserExtensionsContainer::NotifyOfAllActions() {
  if (!page_ || actions_.empty()) {
    return;
  }

  std::vector<extensions_bar::mojom::ExtensionActionInfoPtr> updates;
  for (const auto& [_, action] : actions_) {
    updates.push_back(ToMojom(*action));
  }
  page_->ActionsAddedOrUpdated(std::move(updates));
}

void WebUIBrowserExtensionsContainer::NotifyOfOneAction(
    const ToolbarActionsModel::ActionId& id) {
  if (!page_) {
    return;
  }
  std::vector<extensions_bar::mojom::ExtensionActionInfoPtr> update;
  update.push_back(ToMojom(*actions_[id]));
  page_->ActionsAddedOrUpdated(std::move(update));
}

void WebUIBrowserExtensionsContainer::ExecuteUserAction(const std::string& id) {
  auto it = actions_.find(id);
  CHECK(it != actions_.end());
  it->second->controller()->ExecuteUserAction(
      ToolbarActionViewController::InvocationSource::kToolbarButton);
}

void WebUIBrowserExtensionsContainer::CreateActions() {
  // If the model isn't initialized yet, it will eventually call
  // OnToolbarModelInitialized() and we'll try again.
  if (!model_->actions_initialized()) {
    return;
  }

  for (const auto& action_id : model_->action_ids()) {
    CreateActionForId(action_id);
  }
  NotifyOfAllActions();
}

void WebUIBrowserExtensionsContainer::CreateActionForId(
    const ToolbarActionsModel::ActionId& action_id) {
  auto action_info = std::make_unique<ActionInfo>(
      *this, browser_.get(),
      ExtensionActionViewController::Create(action_id, &browser_.get(), this));
  actions_[action_id] = std::move(action_info);
}
