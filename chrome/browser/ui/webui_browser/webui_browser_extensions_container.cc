// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui_browser/webui_browser_extensions_container.h"

#include <optional>

#include "base/callback_list.h"
#include "base/logging.h"
#include "base/notimplemented.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_view_host.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/extension_action_view_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/views/extensions/extension_action_platform_delegate_views.h"
#include "chrome/browser/ui/webui/util/image_util.h"
#include "chrome/browser/ui/webui_browser/webui_browser_ui.h"
#include "chrome/browser/ui/webui_browser/webui_browser_window.h"
#include "ui/base/models/image_model_utils.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"

namespace {

GURL GetDataUrlForImageModel(ui::ImageModel icon_model,
                             const WebUIBrowserWindow& window) {
  return GURL(webui::EncodePNGAndMakeDataURI(
      icon_model.Rasterize(window.GetColorProvider()),
      window.GetWebUIBrowserUI()->web_ui()->GetDeviceScaleFactor()));
}

}  // namespace

class WebUIBrowserExtensionsContainer::ActionInfo {
 public:
  ActionInfo(WebUIBrowserExtensionsContainer& extensions_container,
             Browser& browser,
             std::unique_ptr<ExtensionActionViewModel> model)
      : extensions_container_(extensions_container),
        browser_(browser),
        model_(std::move(model)),
        model_subscription_(model_->RegisterUpdateObserver(base::BindRepeating(
            &WebUIBrowserExtensionsContainer::NotifyOfOneAction,
            base::Unretained(extensions_container_),
            model_->GetId()))) {}

  ui::TrackedElement* GetAnchor() {
    // TODO(webium): Use the proper button once TrackedElement supports
    // dynamic ids or the like. See https://crbug.com/444237074
    return extensions_container_->window_->GetExtensionsMenuButtonAnchor();
  }

  ExtensionActionViewModel* model() { return model_.get(); }

  extensions_bar::mojom::ExtensionActionInfoPtr ToMojo(
      WebUIBrowserWindow& window) const {
    content::WebContents* web_contents =
        browser_->tab_strip_model()->GetActiveWebContents();
    auto result = extensions_bar::mojom::ExtensionActionInfo::New();
    result->id = model_->GetId();
    result->accessible_name =
        base::UTF16ToUTF8(model_->GetAccessibleName(web_contents));
    result->tooltip = base::UTF16ToUTF8(model_->GetTooltip(web_contents));
    result->is_visible =
        extensions_container_->IsActionVisibleOnToolbar(result->id);

    if (result->is_visible) {
      ui::ImageModel icon_model =
          model_->GetIcon(web_contents, gfx::Size(20, 20));
      if (!model_->IsEnabled(web_contents)) {
        icon_model = ui::GetDefaultDisabledIconFromImageModel(
            icon_model, window.GetColorProvider());
      }
      result->data_url_for_icon = GetDataUrlForImageModel(icon_model, window);
    } else {
      result->data_url_for_icon = GURL("data:,");
    }
    return result;
  }

 private:
  const raw_ref<WebUIBrowserExtensionsContainer> extensions_container_;
  const raw_ref<Browser> browser_;
  std::unique_ptr<ExtensionActionViewModel> model_;
  base::CallbackListSubscription model_subscription_;
};

// This is based on ExtensionContextMenuController.
class WebUIBrowserExtensionsContainer::ContextMenu {
 public:
  static std::unique_ptr<ContextMenu> MaybeCreate(
      WebUIBrowserExtensionsContainer& extensions_container,
      const std::string& action_id) {
    auto it = extensions_container.actions_.find(action_id);
    CHECK(it != extensions_container.actions_.end());

    ui::MenuModel* model = it->second->model()->GetContextMenu(
        extensions::ExtensionContextMenuModel::ContextMenuSource::
            kToolbarAction);

    // It's possible the action doesn't have a context menu.
    if (!model) {
      return nullptr;
    }

    return base::WrapUnique(
        new ContextMenu(action_id, *it->second, model, extensions_container));
  }

  // This is in two steps so that `context_menu_` in the container gets
  // updated.
  void Show(views::Widget* main_widget, ui::mojom::MenuSourceType source) {
    int run_types =
        views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU;

    std::unique_ptr<views::MenuItemView> menu = menu_adapter_->CreateMenu();
    menu_runner_ =
        std::make_unique<views::MenuRunner>(std::move(menu), run_types);

    extensions_container_->OnContextMenuShownFromToolbar(action_id_);

    menu_runner_->RunMenuAt(main_widget, nullptr,
                            action_info_->GetAnchor()->GetScreenBounds(),
                            views::MenuAnchorPosition::kTopLeft, source);
  }

  const std::string& action_id() const { return action_id_; }

 private:
  ContextMenu(const std::string& action_id,
              ActionInfo& action_info,
              ui::MenuModel* model,
              WebUIBrowserExtensionsContainer& extensions_container)
      : action_id_(action_id),
        action_info_(action_info),
        extensions_container_(extensions_container) {
    menu_adapter_ = std::make_unique<views::MenuModelAdapter>(
        model, base::BindRepeating(&ContextMenu::OnMenuClosed,
                                   weak_ptr_factory_.GetWeakPtr()));
  }

  void OnMenuClosed() {
    menu_runner_.reset();
    menu_adapter_.reset();

    // This will delete us.
    extensions_container_->OnContextMenuClosedFromToolbar();
  }

  std::string action_id_;
  const raw_ref<ActionInfo> action_info_;
  const raw_ref<WebUIBrowserExtensionsContainer> extensions_container_;
  std::unique_ptr<views::MenuModelAdapter> menu_adapter_;
  std::unique_ptr<views::MenuRunner> menu_runner_;

  base::WeakPtrFactory<ContextMenu> weak_ptr_factory_{this};
};

WebUIBrowserExtensionsContainer::WebUIBrowserExtensionsContainer(
    Browser& browser,
    WebUIBrowserWindow& window)
    : browser_(browser),
      window_(window),
      model_(*ToolbarActionsModel::Get(browser.profile())),
      extensions_menu_coordinator_(
          std::make_unique<ExtensionsMenuCoordinator>(&browser_.get())) {
  CreateActions();
  observe_actions_.Observe(&model_.get());
}

WebUIBrowserExtensionsContainer::~WebUIBrowserExtensionsContainer() {
  for (const auto& [_, action] : actions_) {
    action->model()->UnregisterCommand();
  }
}

ToolbarActionViewModel* WebUIBrowserExtensionsContainer::GetActionForId(
    const std::string& action_id) {
  auto it = actions_.find(action_id);
  return it != actions_.end() ? it->second->model() : nullptr;
}

std::optional<extensions::ExtensionId>
WebUIBrowserExtensionsContainer::GetPoppedOutActionId() const {
  return popped_out_action_;
}

void WebUIBrowserExtensionsContainer::OnContextMenuShownFromToolbar(
    const std::string& action_id) {
  DCHECK_EQ(action_id, context_menu_->action_id());
  NotifyOfOneAction(action_id);
}

void WebUIBrowserExtensionsContainer::OnContextMenuClosedFromToolbar() {
  std::string prev_context_menu_id = context_menu_->action_id();
  context_menu_.reset();
  NotifyOfOneAction(prev_context_menu_id);
}

bool WebUIBrowserExtensionsContainer::IsActionVisibleOnToolbar(
    const std::string& action_id) const {
  return model_->IsActionPinned(action_id) || popped_out_action_ == action_id ||
         (context_menu_ && context_menu_->action_id() == action_id);
}

void WebUIBrowserExtensionsContainer::UndoPopOut() {
  std::string old_popped_out = std::move(popped_out_action_).value();
  popped_out_action_ = std::nullopt;
  NotifyOfOneAction(old_popped_out);
}

void WebUIBrowserExtensionsContainer::SetPopupOwner(
    ToolbarActionViewModel* popup_owner) {
  // We should never be setting a popup owner when one already exists, and
  // never unsetting one when one wasn't set.
  DCHECK((popup_owner_ != nullptr) ^ (popup_owner != nullptr));
  popup_owner_ = popup_owner;
}

void WebUIBrowserExtensionsContainer::HideActivePopup() {
  if (popup_owner_) {
    popup_owner_->HidePopup();
  }
  DCHECK(!popup_owner_);
}

bool WebUIBrowserExtensionsContainer::CloseOverflowMenuIfOpen() {
  if (extensions_menu_coordinator_->IsShowing()) {
    extensions_menu_coordinator_->Hide();
    return true;
  }
  return false;
}

void WebUIBrowserExtensionsContainer::PopOutAction(
    const extensions::ExtensionId& action_id,
    base::OnceClosure closure) {
  DCHECK(!popped_out_action_.has_value());
  popped_out_action_ = action_id;
  NotifyOfOneAction(action_id);
  NotifyActionPoppedOut(std::move(closure));
}

bool WebUIBrowserExtensionsContainer::ShowToolbarActionPopupForAPICall(
    const std::string& action_id,
    ShowPopupCallback callback) {
  NOTIMPLEMENTED();
  return true;
}

void WebUIBrowserExtensionsContainer::ToggleExtensionsMenu() {
  if (extensions_menu_coordinator_->IsShowing()) {
    extensions_menu_coordinator_->Hide();
  } else {
    extensions_menu_coordinator_->Show(window_->GetExtensionsMenuButtonAnchor(),
                                       this);
  }
}

bool WebUIBrowserExtensionsContainer::HasAnyExtensions() const {
  return !actions_.empty();
}

void WebUIBrowserExtensionsContainer::ShowContextMenuAsFallback(
    const extensions::ExtensionId& action_id) {
  ShowContextMenu(ui::mojom::MenuSourceType::kNone, action_id);
}

void WebUIBrowserExtensionsContainer::OnPopupShown(
    const extensions::ExtensionId& action_id,
    bool by_user) {}

void WebUIBrowserExtensionsContainer::OnPopupClosed(
    const extensions::ExtensionId& action_id) {}

views::FocusManager*
WebUIBrowserExtensionsContainer::GetFocusManagerForAccelerator() {
  return window_->widget()->GetFocusManager();
}

views::BubbleAnchor WebUIBrowserExtensionsContainer::GetReferenceButtonForPopup(
    const extensions::ExtensionId& action_id) {
  auto it = actions_.find(action_id);
  CHECK(it != actions_.end());
  return it->second->GetAnchor();
}

void WebUIBrowserExtensionsContainer::CollapseConfirmation() {
  NOTIMPLEMENTED();
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
  if (popped_out_action_ == id) {
    popped_out_action_ = std::nullopt;
  }
  if (context_menu_ && context_menu_->action_id() == id) {
    context_menu_.reset();
  }
  actions_[id]->model()->UnregisterCommand();
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

  // Don't notify when the window is being destroyed.
  if (!browser_->tab_strip_model()->GetActiveWebContents()) {
    return;
  }

  std::vector<extensions_bar::mojom::ExtensionActionInfoPtr> updates;
  for (const auto& [_, action] : actions_) {
    updates.push_back(action->ToMojo(*window_));
  }
  page_->ActionsAddedOrUpdated(std::move(updates));
}

void WebUIBrowserExtensionsContainer::NotifyOfOneAction(
    const ToolbarActionsModel::ActionId& id) {
  if (!page_) {
    return;
  }

  // Don't notify when the window is being destroyed.
  if (!browser_->tab_strip_model()->GetActiveWebContents()) {
    return;
  }

  std::vector<extensions_bar::mojom::ExtensionActionInfoPtr> update;
  update.push_back(actions_[id]->ToMojo(*window_));
  page_->ActionsAddedOrUpdated(std::move(update));
}

void WebUIBrowserExtensionsContainer::NotifyActionPoppedOut(
    base::OnceClosure closure) {
  if (!page_) {
    std::move(closure).Run();
    return;
  }
  page_->ActionPoppedOut(std::move(closure));
}

void WebUIBrowserExtensionsContainer::ExecuteUserAction(const std::string& id) {
  auto it = actions_.find(id);
  CHECK(it != actions_.end());
  it->second->model()->ExecuteUserAction(
      ToolbarActionViewModel::InvocationSource::kToolbarButton);
}

void WebUIBrowserExtensionsContainer::ShowContextMenu(
    ui::mojom::MenuSourceType source,
    const std::string& id) {
  context_menu_ = ContextMenu::MaybeCreate(*this, id);
  if (context_menu_) {
    context_menu_->Show(window_->widget(), source);
  }
}

void WebUIBrowserExtensionsContainer::ToggleExtensionsMenuFromWebUI() {
  ToggleExtensionsMenu();
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
      ExtensionActionViewModel::Create(
          action_id, &browser_.get(),
          std::make_unique<ExtensionActionPlatformDelegateViews>(
              &browser_.get(), this)));
  action_info->model()->RegisterCommand();
  actions_[action_id] = std::move(action_info);
}
