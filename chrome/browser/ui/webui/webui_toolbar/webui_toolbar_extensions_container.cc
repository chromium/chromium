// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_extensions_container.h"

#include <optional>
#include <utility>

#include "base/callback_list.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/notimplemented.h"
#include "base/numerics/checked_math.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_view_host.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/extensions/extension_action_view_model.h"
#include "chrome/browser/ui/extensions/extensions_toolbar_view_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/extensions/extension_action_delegate_desktop.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_coordinator.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/webui/util/image_util.h"
#include "chrome/browser/ui/webui/webui_toolbar/icon_table.h"
#include "chrome/common/pref_names.h"
#include "components/browser_apis/ui_controllers/toolbar/icon_handle.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/prefs/pref_service.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
#include "content/public/browser/web_ui.h"
#include "extensions/common/extension_features.h"
#include "mojo/public/cpp/bindings/clone_traits.h"
#include "ui/base/base_window.h"
#include "ui/base/models/image_model_utils.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/webui/tracked_element/tracked_element_web_ui.h"

// This is based on ExtensionContextMenuController.
class WebUIToolbarExtensionsContainer::ContextMenu {
 public:
  static std::unique_ptr<ContextMenu> MaybeCreate(
      WebUIToolbarExtensionsContainer& extensions_container,
      const std::string& action_id) {
    ToolbarActionViewModel* action =
        extensions_container.GetActionForId(action_id);
    CHECK(action);

    ui::MenuModel* model =
        action->GetContextMenu(extensions::ExtensionContextMenuModel::
                                   ContextMenuSource::kToolbarAction);

    // It's possible the action doesn't have a context menu.
    if (!model) {
      return nullptr;
    }

    return base::WrapUnique(
        new ContextMenu(action_id, model, extensions_container));
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

    // An unpinned action has no toolbar button (e.g. when triggered via its
    // keyboard shortcut), so fall back to the extensions menu button.
    menu_runner_->RunMenuAt(
        main_widget, nullptr,
        extensions_container_->GetReferenceButtonForPopup(action_id_)
            .GetAnchorRect(),
        views::MenuAnchorPosition::kTopLeft, source);
  }

  const std::string& action_id() const { return action_id_; }

 private:
  ContextMenu(const std::string& action_id,
              ui::MenuModel* model,
              WebUIToolbarExtensionsContainer& extensions_container)
      : action_id_(action_id), extensions_container_(extensions_container) {
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
  const raw_ref<WebUIToolbarExtensionsContainer> extensions_container_;
  std::unique_ptr<views::MenuModelAdapter> menu_adapter_;
  std::unique_ptr<views::MenuRunner> menu_runner_;

  base::WeakPtrFactory<ContextMenu> weak_ptr_factory_{this};
};

class WebUIToolbarExtensionsContainer::ActionInfo {
 public:
  ActionInfo(WebUIToolbarExtensionsContainer& extensions_container,
             BrowserWindowInterface& browser,
             ToolbarActionViewModel& model)
      : extensions_container_(extensions_container),
        browser_(browser),
        model_(model),
        model_subscription_(
            model_->RegisterIconUpdateObserver(base::BindRepeating(
                &WebUIToolbarExtensionsContainer::NotifyOfOneAction,
                base::Unretained(extensions_container_),
                model_->GetId()))) {
    model_->RegisterCommand();
  }

  ~ActionInfo() { model_->UnregisterCommand(); }

  ui::TrackedElement* GetAnchor() {
    return extensions_container_->GetExtensionAnchor(model_->GetId());
  }

  ToolbarActionViewModel* model() { return &model_.get(); }

  // Returns the icon to show in the toolbar for this action on the active tab,
  // greyed out if the action is disabled there.
  ui::ImageModel GetIconModel() {
    content::WebContents* web_contents =
        browser_->GetTabStripModel()->GetActiveWebContents();
    ui::ImageModel icon_model =
        model_->GetIcon(web_contents, gfx::Size(20, 20));
    if (!model_->IsEnabled(web_contents)) {
      icon_model = ui::GetDefaultDisabledIconFromImageModel(
          icon_model, extensions_container_->widget_->GetColorProvider());
    }
    return icon_model;
  }

  extensions_bar::mojom::ExtensionActionInfoPtr ToMojo() {
    content::WebContents* web_contents =
        browser_->GetTabStripModel()->GetActiveWebContents();
    auto result = extensions_bar::mojom::ExtensionActionInfo::New();
    result->id = model_->GetId();
    result->accessible_name = model_->GetAccessibleName(web_contents);
    result->tooltip = model_->GetTooltip(web_contents);
    result->force_visible =
        extensions_container_->ShouldForceVisibility(result->id);
    result->is_visible =
        extensions_container_->IsActionVisibleOnToolbar(result->id);
    result->is_pinned_by_default_iph_anchor =
        extensions_container_->pinned_by_default_iph_extension_id_ ==
        result->id;
    result->highlighted =
        extensions_container_->context_menu_ &&
        extensions_container_->context_menu_->action_id() == result->id;

    if (result->is_visible) {
      icon_handle_ =
          extensions_container_->icon_table_->RegisterImageModelTryReuse(
              GetIconModel(), icon_handle_);
      result->icon = icon_handle_;
    } else {
      icon_handle_ = toolbar_ui_api::IconHandle();
      result->icon = icon_handle_;
    }
    return result;
  }

 private:
  const raw_ref<WebUIToolbarExtensionsContainer> extensions_container_;
  const raw_ref<BrowserWindowInterface> browser_;
  const raw_ref<ToolbarActionViewModel> model_;
  base::CallbackListSubscription model_subscription_;
  toolbar_ui_api::IconHandle icon_handle_;
};

WebUIToolbarExtensionsContainer::AnchoredWidget::AnchoredWidget(
    views::Widget* w,
    std::string id)
    : widget(w), extension_id(std::move(id)) {}

WebUIToolbarExtensionsContainer::AnchoredWidget::~AnchoredWidget() = default;

WebUIToolbarExtensionsContainer::AnchoredWidget::AnchoredWidget(
    AnchoredWidget&&) = default;

WebUIToolbarExtensionsContainer::AnchoredWidget&
WebUIToolbarExtensionsContainer::AnchoredWidget::operator=(AnchoredWidget&&) =
    default;

WebUIToolbarExtensionsContainer::WebUIToolbarExtensionsContainer(
    BrowserWindowInterface& browser,
    views::Widget* widget,
    base::WeakPtr<content::WebContents> web_contents,
    webui_toolbar::IconTable* icon_table,
    bool push_icon_table_updates)
    : browser_(browser),
      widget_(widget),
      web_contents_(web_contents),
      push_icon_table_updates_(push_icon_table_updates),
      icon_table_(icon_table),
      toolbar_view_model_(this,
                          &browser,
                          ToolbarActionsModel::Get(browser.GetProfile())),
      extensions_menu_coordinator_(
          base::FeatureList::IsEnabled(
              extensions_features::kExtensionsMenuAccessControl)
              ? std::make_unique<ExtensionsMenuCoordinator>(
                    &browser,
                    &toolbar_view_model_)
              : nullptr) {
  toolbar_view_model_observation_.Observe(&toolbar_view_model_);
  if (toolbar_view_model_.AreActionsInitialized()) {
    OnActionsInitialized();
  }
}

WebUIToolbarExtensionsContainer::~WebUIToolbarExtensionsContainer() {
  actions_.clear();

  // Create a copy of the anchored widgets, since |anchored_widgets_| will
  // be modified by closing them.
  std::vector<views::Widget*> widgets;
  widgets.reserve(anchored_widgets_.size());
  for (const auto& anchored_widget : anchored_widgets_) {
    widgets.push_back(anchored_widget.widget);
  }
  for (auto* widget : widgets) {
    widget->CloseNow();
  }
  // The widgets should close synchronously (resulting in OnWidgetClosing()),
  // so |anchored_widgets_| should now be empty.
  CHECK(anchored_widgets_.empty(), base::NotFatalUntil::M162);
  CHECK(!views::WidgetObserver::IsInObserverList());
}

void WebUIToolbarExtensionsContainer::SetObserver(
    WebUIToolbarExtensionsContainerObserver* observer) {
  CHECK(!page_);
  observer_ = observer;
}

ToolbarActionViewModel* WebUIToolbarExtensionsContainer::GetActionForId(
    const std::string& action_id) {
  return toolbar_view_model_.GetActionForId(action_id);
}

void WebUIToolbarExtensionsContainer::HideActivePopup() {
  if (popup_owner_) {
    popup_owner_->HidePopup();
  }
  CHECK(!popup_owner_, base::NotFatalUntil::M162);
}

void WebUIToolbarExtensionsContainer::CloseExtensionsMenuIfOpen() {
  if (extensions_menu_coordinator_ &&
      extensions_menu_coordinator_->IsShowing()) {
    extensions_menu_coordinator_->Hide();
  } else if (ExtensionsMenuView::IsShowing()) {
    ExtensionsMenuView::Hide();
  }
}

bool WebUIToolbarExtensionsContainer::ShowToolbarActionPopupForAPICall(
    const std::string& action_id,
    ShowPopupCallback callback) {
  return toolbar_view_model_.ShowToolbarActionPopupForAPICall(
      action_id, std::move(callback));
}

bool WebUIToolbarExtensionsContainer::CanShowToolbarActionPopupForAPICall(
    const ToolbarActionsModel::ActionId& action_id) {
  // Mirrors ExtensionsToolbarDesktop::CanShowToolbarActionPopupForAPICall():
  // don't interrupt an action that's already popped out, and only show popups
  // in the active window.
  return !popped_out_action_ && browser_->GetWindow()->IsActive();
}

std::unique_ptr<ExtensionActionViewModel>
WebUIToolbarExtensionsContainer::CreateActionViewModel(
    const ToolbarActionsModel::ActionId& action_id,
    ExtensionsContainer* extensions_container) {
  return ExtensionActionViewModel::Create(
      action_id, &browser_.get(),
      std::make_unique<ExtensionActionDelegateDesktop>(
          &browser_.get(), extensions_container, this));
}

void WebUIToolbarExtensionsContainer::ToggleExtensionsMenu() {
  if (extensions_menu_coordinator_ &&
      extensions_menu_coordinator_->IsShowing()) {
    extensions_menu_coordinator_->Hide();
    return;
  } else if (ExtensionsMenuView::IsShowing()) {
    ExtensionsMenuView::Hide();
    return;
  }

  if (extensions_menu_coordinator_) {
    extensions_menu_coordinator_->Show(
        views::BubbleAnchor(GetExtensionsMenuButtonAnchor()), this);
  } else {
    ExtensionsMenuView::ShowBubble(
        views::BubbleAnchor(GetExtensionsMenuButtonAnchor()), &browser_.get(),
        &toolbar_view_model_, this);
  }
}

bool WebUIToolbarExtensionsContainer::HasAnyExtensions() const {
  return toolbar_view_model_.HasAnyExtensions();
}

void WebUIToolbarExtensionsContainer::ShowPinnedByDefaultIPH(
    const std::string& extension_id) {
  if (!base::FeatureList::IsEnabled(features::kExtensionsPinnedByDefault) ||
      !browser_->GetProfile()->GetPrefs()->GetBoolean(
          prefs::kExtensionsPinnedByDefault)) {
    return;
  }

  if (!actions_.contains(extension_id) ||
      !IsActionVisibleOnToolbar(extension_id)) {
    return;
  }

  // Don't interrupt a popped out action (which may be waiting to show a popup,
  // or already showing one) to show the IPH.
  if (popped_out_action_) {
    return;
  }

  // Only one extension can be the anchor at a time. If there's already an IPH
  // request, whether it's waiting for its anchor, queued, or showing, keep it
  // and ignore this one. Replacing it would require aborting the earlier promo,
  // and the IPH system only allows one instance of a promo to be queued or
  // shown at a time.
  if (pinned_by_default_iph_extension_id_) {
    return;
  }

  // This causes the WebUI button to register
  // kExtensionsPinnedByDefaultElementId, and forces the button to be visible,
  // even if it would otherwise be hidden due to overflow (see
  // ShouldForceVisibility()).
  pinned_by_default_iph_extension_id_ = extension_id;
  NotifyOfOneAction(extension_id);

  // Wait for the button to register kExtensionsPinnedByDefaultElementId and,
  // like views, for any animations to finish so that the button is in its
  // final location.
  NotifyActionPoppedOut(
      extension_id,
      base::BindOnce(
          &WebUIToolbarExtensionsContainer::ShowPinnedByDefaultIPHNow,
          weak_ptr_factory_.GetWeakPtr(), extension_id));
}

void WebUIToolbarExtensionsContainer::ShowPinnedByDefaultIPHNow(
    const std::string& extension_id) {
  // Bail if the anchor has since been cleared or moved to another extension.
  if (pinned_by_default_iph_extension_id_ != extension_id) {
    return;
  }

  // Clears the anchor, unless it has since moved to another extension.
  auto clear_anchor = base::BindRepeating(
      [](base::WeakPtr<WebUIToolbarExtensionsContainer> container,
         const std::string& extension_id) {
        if (container &&
            container->pinned_by_default_iph_extension_id_ == extension_id) {
          container->ClearPinnedByDefaultIphAnchor();
        }
      },
      weak_ptr_factory_.GetWeakPtr(), extension_id);

  user_education::FeaturePromoParams params(
      feature_engagement::kIPHExtensionsPinnedByDefaultFeature);
  params.close_callback = base::BindOnce(clear_anchor);
  params.show_promo_result_callback = base::BindOnce(
      [](base::RepeatingClosure clear_anchor,
         user_education::FeaturePromoResult result) {
        if (!result) {
          clear_anchor.Run();
        }
      },
      clear_anchor);
  BrowserUserEducationInterface::From(&browser_.get())
      ->MaybeShowFeaturePromo(std::move(params));
}

void WebUIToolbarExtensionsContainer::ClearPinnedByDefaultIphAnchor() {
  std::optional<std::string> old_extension_id =
      std::exchange(pinned_by_default_iph_extension_id_, std::nullopt);
  if (!old_extension_id) {
    return;
  }
  if (actions_.contains(*old_extension_id)) {
    NotifyOfOneAction(*old_extension_id);
  }
  // Inform `observer_` of the cancellation. The IPH pop out callback, if any,
  // is run. If there is a callback, that means ShowPinnedByDefaultIPHNow() will
  // be called, which does nothing, since `pinned_by_default_iph_extension_id_`
  // has been cleared.
  //
  // The callback is run rather than destroyed because the wrapper treats IPH
  // and PopOutAction() callbacks the same, and the latter must always be run.
  //
  // If there's no pending IPH callback, this is a no-op, since there can't be a
  // pending PopOutAction() callback while `pinned_by_default_iph_extension_id_`
  // is set.
  if (observer_) {
    observer_->OnPopOutCancelled();
  }
}

void WebUIToolbarExtensionsContainer::CancelPinnedByDefaultIph() {
  if (!pinned_by_default_iph_extension_id_) {
    return;
  }
  // Clear the anchor first, so the promo's callbacks, which may be run
  // synchronously when it's aborted, do nothing.
  ClearPinnedByDefaultIphAnchor();
  // If the promo has already been requested, remove it from the queue, or close
  // it if it's showing. Otherwise, this does nothing.
  if (BrowserUserEducationInterface* user_education =
          BrowserUserEducationInterface::From(&browser_.get())) {
    user_education->AbortFeaturePromo(
        feature_engagement::kIPHExtensionsPinnedByDefaultFeature);
  }
}

std::optional<extensions::ExtensionId>
WebUIToolbarExtensionsContainer::GetPoppedOutActionId() const {
  return popped_out_action_;
}

bool WebUIToolbarExtensionsContainer::IsVisible() const {
  return GetWidget() && GetWidget()->IsVisible() && !actions_.empty();
}

void WebUIToolbarExtensionsContainer::OnContextMenuShownFromToolbar(
    const std::string& action_id) {
  CHECK_EQ(action_id, context_menu_->action_id(), base::NotFatalUntil::M162);
  NotifyOfOneAction(action_id);
}

void WebUIToolbarExtensionsContainer::OnContextMenuClosedFromToolbar() {
  std::string prev_context_menu_id = context_menu_->action_id();
  context_menu_.reset();
  NotifyOfOneAction(prev_context_menu_id);
}

bool WebUIToolbarExtensionsContainer::IsActionVisibleOnToolbar(
    const std::string& action_id) const {
  return std::ranges::contains(toolbar_view_model_.GetPinnedActionIds(),
                               action_id) ||
         ShouldForceVisibility(action_id);
}

void WebUIToolbarExtensionsContainer::UndoPopOut() {
  std::string old_popped_out = std::move(popped_out_action_).value();
  popped_out_action_ = std::nullopt;
  NotifyOfOneAction(old_popped_out);
  // This synchronously runs the pending PopOutAction() callback, if any.
  if (observer_) {
    observer_->OnPopOutCancelled();
  }
}

void WebUIToolbarExtensionsContainer::SetPopupOwner(
    ToolbarActionViewModel* popup_owner) {
  // We should never be setting a popup owner when one already exists, and
  // never unsetting one when one wasn't set.
  CHECK((popup_owner_ != nullptr) ^ (popup_owner != nullptr),
        base::NotFatalUntil::M162);
  popup_owner_ = popup_owner;
}

void WebUIToolbarExtensionsContainer::PopOutAction(
    const extensions::ExtensionId& action_id,
    base::OnceClosure closure) {
  CHECK(!popped_out_action_.has_value(), base::NotFatalUntil::M162);
  // Cancel any IPH, since popped out actions take priority over it.
  CancelPinnedByDefaultIph();
  popped_out_action_ = action_id;
  NotifyOfOneAction(action_id);
  NotifyActionPoppedOut(action_id, std::move(closure));
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
  if (ui::TrackedElement* anchor = GetExtensionAnchor(action_id)) {
    return views::BubbleAnchor(anchor);
  }
  return GetExtensionsButtonAnchor();
}

views::BubbleAnchor
WebUIToolbarExtensionsContainer::GetExtensionsButtonAnchor() {
  if (ui::TrackedElement* anchor = GetExtensionsMenuButtonAnchor()) {
    return views::BubbleAnchor(anchor);
  }
  if (BrowserView* browser_view =
          BrowserView::GetBrowserViewForBrowser(&browser_.get())) {
    if (browser_view->toolbar()) {
      return views::BubbleAnchor(browser_view->toolbar());
    }
  }
  return views::BubbleAnchor(GetWidget()->GetRootView());
}

views::BubbleBorder::Arrow WebUIToolbarExtensionsContainer::GetPopupArrow()
    const {
  return views::BubbleBorder::TOP_RIGHT;
}

void WebUIToolbarExtensionsContainer::CollapseConfirmation() {
  NOTIMPLEMENTED();
}

void WebUIToolbarExtensionsContainer::OnActionsInitialized() {
  CHECK(actions_.empty());
  CreateActions();
}

void WebUIToolbarExtensionsContainer::OnActionAdded(
    const ToolbarActionsModel::ActionId& id) {
  CreateActionForId(id);
  NotifyOfOneAction(id);
}

void WebUIToolbarExtensionsContainer::OnActionRemoved(
    const ToolbarActionsModel::ActionId& id) {
  bool pop_out_cancelled = false;
  if (popped_out_action_ == id) {
    popped_out_action_ = std::nullopt;
    pop_out_cancelled = true;
  }
  if (context_menu_ && context_menu_->action_id() == id) {
    context_menu_.reset();
  }
  // The action may be removed (e.g. the extension uninstalled) while its popup
  // is showing. ToolbarActionView does this in its destructor for the Views
  // container; there's no per-action view here, so hide the popup explicitly
  // before destroying the model, which expects not to be showing a popup.
  // `popped_out_action_` was cleared above, so this doesn't re-enter
  // UndoPopOut().
  auto it = actions_.find(id);
  CHECK(it != actions_.end());
  it->second->model()->HidePopup();
  actions_.erase(it);
  if (pinned_by_default_iph_extension_id_ == id) {
    ClearPinnedByDefaultIphAnchor();
  }

  std::vector<toolbar_ui_api::mojom::IconUpdatePtr> icon_updates;
  if (push_icon_table_updates_) {
    icon_updates = icon_table_->TakePendingUpdates();
  }

  if (page_) {
    page_->ActionRemoved(std::move(icon_updates), id);
  } else if (observer_) {
    observer_->OnActionRemoved(std::move(icon_updates), id);
  }

  // Notify after the action has been fully removed. This also means that the
  // pending PopOutAction() callback, if bound to the removed action's delegate,
  // will be a no-op.
  if (pop_out_cancelled && observer_) {
    observer_->OnPopOutCancelled();
  }
}

void WebUIToolbarExtensionsContainer::OnActionUpdated(
    const ToolbarActionsModel::ActionId& id) {
  NotifyOfOneAction(id);
}

void WebUIToolbarExtensionsContainer::OnPinnedActionsChanged() {
  NotifyOfAllActions();
}

void WebUIToolbarExtensionsContainer::OnActiveWebContentsChanged(
    bool is_same_document,
    content::WebContents* web_contents) {
  if (is_same_document) {
    return;
  }
  NotifyOfAllActions();
}

void WebUIToolbarExtensionsContainer::Bind(
    mojo::PendingRemote<extensions_bar::mojom::Page> page,
    mojo::PendingReceiver<extensions_bar::mojom::PageHandler> receiver) {
  CHECK(!observer_);
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
  page_.reset();
  page_.Bind(std::move(page));
  if (push_icon_table_updates_) {
    page_->ActionsAddedOrUpdated(
        icon_table_->GetFullState(),
        std::vector<extensions_bar::mojom::ExtensionActionInfoPtr>());
  }
  NotifyOfAllActions();
}

void WebUIToolbarExtensionsContainer::NotifyOfAllActions() {
  if (!page_ && !observer_) {
    return;
  }
  if (actions_.empty()) {
    return;
  }

  // Don't notify when the window is being destroyed.
  if (!browser_->GetTabStripModel()->GetActiveWebContents()) {
    return;
  }

  std::vector<extensions_bar::mojom::ExtensionActionInfoPtr> updates;
  for (const auto& id : GetOrderedActionIds()) {
    auto it = actions_.find(id);
    if (it != actions_.end()) {
      updates.push_back(it->second->ToMojo());
    }
  }
  std::vector<toolbar_ui_api::mojom::IconUpdatePtr> icon_updates;
  if (push_icon_table_updates_) {
    icon_updates = icon_table_->TakePendingUpdates();
  }

  if (page_) {
    page_->ActionsAddedOrUpdated(std::move(icon_updates), std::move(updates));
  } else if (observer_) {
    observer_->OnActionsAddedOrUpdated(std::move(icon_updates),
                                       std::move(updates));
  }
}

void WebUIToolbarExtensionsContainer::NotifyOfOneAction(
    const ToolbarActionsModel::ActionId& id) {
  if (!page_ && !observer_) {
    return;
  }

  // Don't notify when the window is being destroyed.
  if (!browser_->GetTabStripModel()->GetActiveWebContents()) {
    return;
  }

  std::vector<extensions_bar::mojom::ExtensionActionInfoPtr> update;
  update.push_back(actions_[id]->ToMojo());
  std::vector<toolbar_ui_api::mojom::IconUpdatePtr> icon_updates;
  if (push_icon_table_updates_) {
    icon_updates = icon_table_->TakePendingUpdates();
  }

  if (page_) {
    page_->ActionsAddedOrUpdated(std::move(icon_updates), std::move(update));
  } else if (observer_) {
    observer_->OnActionsAddedOrUpdated(std::move(icon_updates),
                                       std::move(update));
  }
}

ui::TrackedElement*
WebUIToolbarExtensionsContainer::GetExtensionsMenuButtonAnchor() const {
  return GetExtensionAnchor("");
}

ui::ElementIdentifier WebUIToolbarExtensionsContainer::GetElementId(
    std::string_view extension_id) {
  return extension_id.empty() ? kExtensionsMenuButtonElementId
                              : kToolbarActionViewElementId;
}

std::string WebUIToolbarExtensionsContainer::GetSecondaryElementId(
    std::string_view extension_id) {
  return base::StrCat({"ext:", extension_id});
}

ui::TrackedElement* WebUIToolbarExtensionsContainer::GetExtensionAnchor(
    std::string_view extension_id) const {
  return GetExtensionElement(GetElementId(extension_id), extension_id);
}

ui::TrackedElement* WebUIToolbarExtensionsContainer::GetExtensionElement(
    ui::ElementIdentifier element_id,
    std::string_view extension_id) const {
  const std::string secondary_id = GetSecondaryElementId(extension_id);
  for (ui::TrackedElement* element :
       ui::ElementTracker::GetElementTracker()->GetAllMatchingElements(
           element_id,
           views::ElementTrackerViews::GetContextForWidget(GetWidget()))) {
    if (element->GetSecondaryIdentifier() == secondary_id) {
      return element;
    }
  }
  return nullptr;
}

views::Widget* WebUIToolbarExtensionsContainer::GetWidget() const {
  return widget_;
}

gfx::ImageSkia WebUIToolbarExtensionsContainer::GetActionIconForTesting(
    const std::string& action_id) {
  auto it = actions_.find(action_id);
  CHECK(it != actions_.end());
  return it->second->GetIconModel().Rasterize(widget_->GetColorProvider());
}

void WebUIToolbarExtensionsContainer::NotifyActionPoppedOut(
    const std::string& action_id,
    base::OnceClosure closure) {
  if (page_) {
    page_->ActionPoppedOut(std::move(closure));
  } else if (observer_) {
    observer_->OnActionPoppedOut(action_id, std::move(closure));
  } else {
    std::move(closure).Run();
  }
}

void WebUIToolbarExtensionsContainer::ExecuteUserAction(const std::string& id) {
  toolbar_view_model_.ExecuteUserAction(
      id, ToolbarActionViewModel::InvocationSource::kToolbarButton);
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

void WebUIToolbarExtensionsContainer::MoveExtensionAction(
    const std::string& extension_id,
    int32_t target_index) {
  const auto& pinned_action_ids = toolbar_view_model_.GetPinnedActionIds();
  auto iter = std::ranges::find(pinned_action_ids, extension_id);
  if (iter == pinned_action_ids.end()) {
    return;
  }
  if (target_index < 0 ||
      target_index >= static_cast<int32_t>(pinned_action_ids.size())) {
    return;
  }
  toolbar_view_model_.MovePinnedAction(extension_id, target_index);
}

void WebUIToolbarExtensionsContainer::MoveExtensionActionBy(
    const std::string& extension_id,
    int32_t delta) {
  const auto& pinned_action_ids = toolbar_view_model_.GetPinnedActionIds();
  auto iter = std::ranges::find(pinned_action_ids, extension_id);
  if (iter == pinned_action_ids.end()) {
    return;
  }
  ptrdiff_t current_index = std::distance(pinned_action_ids.begin(), iter);
  base::CheckedNumeric<int32_t> checked_target_index = current_index;
  checked_target_index += delta;
  int32_t target_index;
  if (!checked_target_index.AssignIfValid(&target_index)) {
    return;
  }
  if (target_index >= 0 &&
      target_index < static_cast<int32_t>(pinned_action_ids.size())) {
    toolbar_view_model_.MovePinnedAction(extension_id, target_index);
  }
}

std::vector<std::string> WebUIToolbarExtensionsContainer::GetOrderedActionIds()
    const {
  std::vector<std::string> ordered;
  base::flat_set<std::string> added;
  for (const auto& id : toolbar_view_model_.GetPinnedActionIds()) {
    ordered.push_back(id);
    added.insert(id);
  }
  for (const auto& action_id : toolbar_view_model_.GetAllActionIds()) {
    if (IsActionVisibleOnToolbar(action_id) && !added.contains(action_id)) {
      ordered.push_back(action_id);
      added.insert(action_id);
    }
  }
  for (const auto& action_id : toolbar_view_model_.GetAllActionIds()) {
    if (!added.contains(action_id)) {
      ordered.push_back(action_id);
    }
  }
  return ordered;
}

void WebUIToolbarExtensionsContainer::CreateActions() {
  // If the model isn't initialized yet, it will eventually call
  // OnActionsInitialized() and we'll try again.
  if (!toolbar_view_model_.AreActionsInitialized()) {
    return;
  }

  for (const auto& action_id : toolbar_view_model_.GetAllActionIds()) {
    CreateActionForId(action_id);
  }
  NotifyOfAllActions();
}

void WebUIToolbarExtensionsContainer::CreateActionForId(
    const ToolbarActionsModel::ActionId& action_id) {
  ToolbarActionViewModel* model =
      toolbar_view_model_.GetActionModelForId(action_id);
  CHECK(model);
  actions_[action_id] =
      std::make_unique<ActionInfo>(*this, browser_.get(), *model);
}

void WebUIToolbarExtensionsContainer::ShowWidgetForExtension(
    views::Widget* widget,
    const std::string& extension_id) {
  ui::TrackedElement* anchor = GetExtensionAnchor(extension_id);
  if (anchor) {
    anchored_widgets_.emplace_back(widget, extension_id);
    widget->AddObserver(this);
    NotifyOfOneAction(extension_id);
    AnchorAndShowWidgetImmediately(widget, anchor);
  } else {
    // If the particular extension button isn't anchorable, it's likely not yet
    // visible. Adding it to anchored_widgets_ will make it visible, but we'll
    // have to wait for it to animate in for it to be anchorable. Delay calling
    // AnchorAndShowWidgetImmediately until it has finished animating in.

    // Clear the bubble's anchor to avoid dangling pointers if the
    // TrackedElement goes away while we're delaying.
    if (views::BubbleDialogDelegate* bubble_delegate =
            widget->widget_delegate()->AsBubbleDialogDelegate()) {
      bubble_delegate->SetAnchor(views::BubbleAnchor());
    }

    auto subscription =
        ui::ElementTracker::GetElementTracker()->AddElementShownCallback(
            GetElementId(extension_id),
            views::ElementTrackerViews::GetContextForWidget(GetWidget()),
            base::BindRepeating(&WebUIToolbarExtensionsContainer::
                                    AnchorAndShowWidgetImmediately,
                                base::Unretained(this),
                                base::Unretained(widget)));

    AnchoredWidget anchored_widget(widget, extension_id);
    anchored_widget.subscription = std::move(subscription);
    anchored_widgets_.push_back(std::move(anchored_widget));
    widget->AddObserver(this);
    NotifyOfOneAction(extension_id);
  }
}

void WebUIToolbarExtensionsContainer::OnWidgetDestroying(
    views::Widget* widget) {
  auto iter =
      std::ranges::find(anchored_widgets_, widget, &AnchoredWidget::widget);
  CHECK(iter != anchored_widgets_.end());
  iter->widget->RemoveObserver(this);
  const std::string extension_id = std::move(iter->extension_id);
  anchored_widgets_.erase(iter);
  if (actions_.find(extension_id) != actions_.end()) {
    NotifyOfOneAction(extension_id);
  }
}

void WebUIToolbarExtensionsContainer::AnchorAndShowWidgetImmediately(
    views::Widget* widget,
    ui::TrackedElement* unused_anchor) {
  auto iter =
      std::ranges::find(anchored_widgets_, widget, &AnchoredWidget::widget);

  if (iter == anchored_widgets_.end()) {
    return;
  }

  ui::TrackedElement* anchor = GetExtensionAnchor(iter->extension_id);
  if (!anchor) {
    // This shown notification was about another extension button.
    // Keep waiting for `iter->extension_id`'s button.
    return;
  }

  iter->subscription = {};

  if (views::BubbleDialogDelegate* bubble_delegate =
          widget->widget_delegate()->AsBubbleDialogDelegate()) {
    bubble_delegate->SetAnchor(views::BubbleAnchor(anchor));
  }
  widget->Show();
}

bool WebUIToolbarExtensionsContainer::ShouldForceVisibility(
    const std::string& action_id) const {
  if (popped_out_action_ == action_id ||
      (context_menu_ && context_menu_->action_id() == action_id)) {
    return true;
  }

  // Unlike Views, where the pinned-by-default IPH fails to show if its anchor
  // has overflowed, force the IPH's anchor to be visible. Otherwise, the IPH
  // would remain pending until the button fits on the toolbar, since
  // ShowPinnedByDefaultIPH() waits for the button to be shown.
  if (pinned_by_default_iph_extension_id_ == action_id) {
    return true;
  }

  for (const auto& anchored_widget : anchored_widgets_) {
    if (anchored_widget.extension_id == action_id) {
      return true;
    }
  }

  return false;
}
