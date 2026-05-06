// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_extensions_container.h"

#include <optional>

#include "base/callback_list.h"
#include "base/logging.h"
#include "base/notimplemented.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_view_host.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/extension_action_view_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/views/extensions/extension_action_delegate_desktop.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_coordinator.h"
#include "chrome/browser/ui/webui/util/image_util.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/models/image_model_utils.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"

namespace {

GURL GetDataUrlForImageModel(ui::ImageModel icon_model,
                             const ui::ColorProvider* color_provider,
                             base::WeakPtr<content::WebContents> web_contents) {
  DCHECK(web_contents);
  return GURL(webui::EncodePNGAndMakeDataURI(
      icon_model.Rasterize(color_provider),
      web_contents ? web_contents->GetWebUI()->GetDeviceScaleFactor() : 1.0f));
}

}  // namespace

class WebUIToolbarExtensionsContainer::ActionInfo {
 public:
  ActionInfo(WebUIToolbarExtensionsContainer& extensions_container,
             Browser& browser,
             std::unique_ptr<ExtensionActionViewModel> model)
      : extensions_container_(extensions_container),
        browser_(browser),
        model_(std::move(model)),
        model_subscription_(
            model_->RegisterIconUpdateObserver(base::BindRepeating(
                &WebUIToolbarExtensionsContainer::NotifyOfOneAction,
                base::Unretained(extensions_container_),
                model_->GetId()))) {}

  ui::TrackedElement* GetAnchor() {
    // TODO(webium): Use the proper button once TrackedElement supports
    // dynamic ids or the like. See https://crbug.com/444237074
    return extensions_container_->GetExtensionsMenuButtonAnchor();
  }

  ExtensionActionViewModel* model() { return model_.get(); }

  extensions_bar::mojom::ExtensionActionInfoPtr ToMojo(
      BrowserWindow& window) const {
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
      result->data_url_for_icon = GetDataUrlForImageModel(
          icon_model, extensions_container_->window_->GetColorProvider(),
          extensions_container_->web_contents_);
    } else {
      result->data_url_for_icon = GURL("data:,");
    }
    return result;
  }

 private:
  const raw_ref<WebUIToolbarExtensionsContainer> extensions_container_;
  const raw_ref<Browser> browser_;
  std::unique_ptr<ExtensionActionViewModel> model_;
  base::CallbackListSubscription model_subscription_;
};

// This is based on ExtensionContextMenuController.
class WebUIToolbarExtensionsContainer::ContextMenu {
 public:
  static std::unique_ptr<ContextMenu> MaybeCreate(
      WebUIToolbarExtensionsContainer& extensions_container,
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
              WebUIToolbarExtensionsContainer& extensions_container)
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
  const raw_ref<WebUIToolbarExtensionsContainer> extensions_container_;
  std::unique_ptr<views::MenuModelAdapter> menu_adapter_;
  std::unique_ptr<views::MenuRunner> menu_runner_;

  base::WeakPtrFactory<ContextMenu> weak_ptr_factory_{this};
};

WebUIToolbarExtensionsContainer::WebUIToolbarExtensionsContainer(
    Browser& browser,
    BrowserWindow& window,
    base::WeakPtr<content::WebContents> web_contents)
    : browser_(browser),
      window_(window),
      web_contents_(web_contents),
      model_(*ToolbarActionsModel::Get(browser.profile())),
      extensions_menu_coordinator_(
          std::make_unique<ExtensionsMenuCoordinator>(&browser_.get(), this)) {
  CreateActions();
  observe_actions_.Observe(&model_.get());
}

WebUIToolbarExtensionsContainer::~WebUIToolbarExtensionsContainer() {
  for (const auto& [_, action] : actions_) {
    action->model()->UnregisterCommand();
  }
}

ToolbarActionViewModel* WebUIToolbarExtensionsContainer::GetActionForId(
    const std::string& action_id) {
  auto it = actions_.find(action_id);
  return it != actions_.end() ? it->second->model() : nullptr;
}

void WebUIToolbarExtensionsContainer::HideActivePopup() {
  if (popup_owner_) {
    popup_owner_->HidePopup();
  }
  DCHECK(!popup_owner_);
}

void WebUIToolbarExtensionsContainer::CloseExtensionsMenuIfOpen() {
  if (extensions_menu_coordinator_->IsShowing()) {
    extensions_menu_coordinator_->Hide();
  }
}

bool WebUIToolbarExtensionsContainer::ShowToolbarActionPopupForAPICall(
    const std::string& action_id,
    ShowPopupCallback callback) {
  NOTIMPLEMENTED();
  return true;
}

void WebUIToolbarExtensionsContainer::ToggleExtensionsMenu() {
  if (extensions_menu_coordinator_->IsShowing()) {
    extensions_menu_coordinator_->Hide();
  } else {
    extensions_menu_coordinator_->Show(
        views::BubbleAnchor(GetExtensionsMenuButtonAnchor()), this);
  }
}

bool WebUIToolbarExtensionsContainer::HasAnyExtensions() const {
  return !actions_.empty();
}

std::optional<extensions::ExtensionId>
WebUIToolbarExtensionsContainer::GetPoppedOutActionId() const {
  return popped_out_action_;
}

void WebUIToolbarExtensionsContainer::OnContextMenuShownFromToolbar(
    const std::string& action_id) {
  DCHECK_EQ(action_id, context_menu_->action_id());
  NotifyOfOneAction(action_id);
}

void WebUIToolbarExtensionsContainer::OnContextMenuClosedFromToolbar() {
  std::string prev_context_menu_id = context_menu_->action_id();
  context_menu_.reset();
  NotifyOfOneAction(prev_context_menu_id);
}

bool WebUIToolbarExtensionsContainer::IsActionVisibleOnToolbar(
    const std::string& action_id) const {
  return model_->IsActionPinned(action_id) || popped_out_action_ == action_id ||
         (context_menu_ && context_menu_->action_id() == action_id);
}

void WebUIToolbarExtensionsContainer::UndoPopOut() {
  std::string old_popped_out = std::move(popped_out_action_).value();
  popped_out_action_ = std::nullopt;
  NotifyOfOneAction(old_popped_out);
}

void WebUIToolbarExtensionsContainer::SetPopupOwner(
    ToolbarActionViewModel* popup_owner) {
  // We should never be setting a popup owner when one already exists, and
  // never unsetting one when one wasn't set.
  DCHECK((popup_owner_ != nullptr) ^ (popup_owner != nullptr));
  popup_owner_ = popup_owner;
}

void WebUIToolbarExtensionsContainer::PopOutAction(
    const extensions::ExtensionId& action_id,
    base::OnceClosure closure) {
  DCHECK(!popped_out_action_.has_value());
  popped_out_action_ = action_id;
  NotifyOfOneAction(action_id);
  NotifyActionPoppedOut(std::move(closure));
}

void WebUIToolbarExtensionsContainer::ShowContextMenuAsFallback(
    const extensions::ExtensionId& action_id) {
  ShowContextMenu(ui::mojom::MenuSourceType::kNone, action_id);
}

void WebUIToolbarExtensionsContainer::OnPopupShown(
    const extensions::ExtensionId& action_id,
    bool by_user) {}

void WebUIToolbarExtensionsContainer::OnPopupClosed(
    const extensions::ExtensionId& action_id) {}

views::FocusManager*
WebUIToolbarExtensionsContainer::GetFocusManagerForAccelerator() {
  return GetWidget()->GetFocusManager();
}

views::BubbleAnchor WebUIToolbarExtensionsContainer::GetReferenceButtonForPopup(
    const extensions::ExtensionId& action_id) {
  auto it = actions_.find(action_id);
  CHECK(it != actions_.end());
  return views::BubbleAnchor(it->second->GetAnchor());
}

void WebUIToolbarExtensionsContainer::CollapseConfirmation() {
  NOTIMPLEMENTED();
}

void WebUIToolbarExtensionsContainer::OnToolbarModelInitialized() {
  CreateActions();
}

void WebUIToolbarExtensionsContainer::OnToolbarActionAdded(
    const ToolbarActionsModel::ActionId& id) {
  CreateActionForId(id);
  NotifyOfOneAction(id);
}

void WebUIToolbarExtensionsContainer::OnToolbarActionRemoved(
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

void WebUIToolbarExtensionsContainer::OnToolbarActionUpdated(
    const ToolbarActionsModel::ActionId& id) {
  NotifyOfOneAction(id);
}

void WebUIToolbarExtensionsContainer::OnToolbarPinnedActionsChanged() {
  NotifyOfAllActions();
}

void WebUIToolbarExtensionsContainer::Bind(
    mojo::PendingRemote<extensions_bar::mojom::Page> page,
    mojo::PendingReceiver<extensions_bar::mojom::PageHandler> receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
  page_.reset();
  page_.Bind(std::move(page));
  NotifyOfAllActions();
}

void WebUIToolbarExtensionsContainer::NotifyOfAllActions() {
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

void WebUIToolbarExtensionsContainer::NotifyOfOneAction(
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

ui::TrackedElement*
WebUIToolbarExtensionsContainer::GetExtensionsMenuButtonAnchor() const {
  return ui::ElementTracker::GetElementTracker()->GetFirstMatchingElement(
      kExtensionsMenuButtonElementId,
      views::ElementTrackerViews::GetContextForWidget(GetWidget()));
}

views::Widget* WebUIToolbarExtensionsContainer::GetWidget() const {
  return views::Widget::GetWidgetForNativeWindow(window_->GetNativeWindow());
}

void WebUIToolbarExtensionsContainer::NotifyActionPoppedOut(
    base::OnceClosure closure) {
  if (!page_) {
    std::move(closure).Run();
    return;
  }
  page_->ActionPoppedOut(std::move(closure));
}

void WebUIToolbarExtensionsContainer::ExecuteUserAction(const std::string& id) {
  auto it = actions_.find(id);
  CHECK(it != actions_.end());
  it->second->model()->ExecuteUserAction(
      ToolbarActionViewModel::InvocationSource::kToolbarButton);
}

void WebUIToolbarExtensionsContainer::ShowContextMenu(
    ui::mojom::MenuSourceType source,
    const std::string& id) {
  context_menu_ = ContextMenu::MaybeCreate(*this, id);
  if (context_menu_) {
    context_menu_->Show(GetWidget(), source);
  }
}

void WebUIToolbarExtensionsContainer::ToggleExtensionsMenuFromWebUI() {
  ToggleExtensionsMenu();
}

void WebUIToolbarExtensionsContainer::CreateActions() {
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

void WebUIToolbarExtensionsContainer::CreateActionForId(
    const ToolbarActionsModel::ActionId& action_id) {
  auto action_info = std::make_unique<ActionInfo>(
      *this, browser_.get(),
      ExtensionActionViewModel::Create(
          action_id, &browser_.get(),
          std::make_unique<ExtensionActionDelegateDesktop>(&browser_.get(),
                                                           this, this)));
  action_info->model()->RegisterCommand();
  actions_[action_id] = std::move(action_info);
}
