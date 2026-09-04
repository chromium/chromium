// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/webui_page_action_control.h"

#include <utility>
#include <variant>

#include "base/functional/bind.h"
#include "base/memory/raw_ref.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "base/types/expected.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/page_action/action_ids.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "chrome/browser/ui/page_action/page_action_model.h"
#include "chrome/browser/ui/page_action/page_action_properties_provider.h"
#include "chrome/browser/ui/page_action/page_action_triggers.h"
#include "chrome/browser/ui/side_panel/side_panel_action_callback.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_reopen_suppressor.h"
#include "chrome/browser/ui/views/page_action/anchored_message_view.h"
#include "chrome/browser/ui/views/page_action/page_action_view_util.h"
#include "chrome/browser/ui/views/page_action/webui_page_action_view.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/browser/ui/webui/webui_toolbar/utils/toolbar_button_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/mojom/base/error.mojom.h"
#include "ui/actions/actions.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_id.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/widget/widget.h"

namespace page_actions {

using MojomPageActionId = toolbar_ui_api::mojom::PageActionId;

namespace {

using Code = mojo_base::mojom::Code;
using Error = mojo_base::mojom::Error;

}  // namespace

// Instantiated per ActionId.
class WebUIPageActionControl::WebUIPageActionDelegate
    : public page_actions::PageActionController::Delegate,
      public page_actions::PageActionModelObserver,
      public page_actions::AnchoredMessageBubbleView::Delegate {
 public:
  WebUIPageActionDelegate(actions::ActionId action_id,
                          actions::ActionItem& action_item,
                          WebUIPageActionControl& owner)
      : action_id_(action_id), action_item_(action_item), owner_(owner) {}
  ~WebUIPageActionDelegate() override;

  // Updates the observed PageActionController (e.g. when the active tab
  // changes), resetting existing observations and any active anchored message.
  void SetController(page_actions::PageActionController* controller);
  void UpdateStateAndNotify();

  // page_actions::PageActionController::Delegate:
  void SetIsChipShowingChangedCallback(
      IsChipShowingChangedCallback callback) override {
    is_chip_showing_changed_callback_ = std::move(callback);
  }
  void SetImageAnimationStartedCallback(
      ImageAnimationStartedCallback callback) override {
    image_animation_started_callback_ = std::move(callback);
  }
  void SetAnchoredMessageCloseCallback(
      base::RepeatingClosure callback) override {
    anchored_message_close_callback_ = std::move(callback);
  }
  void SetAnchoredMessageExpandCallback(
      base::RepeatingClosure callback) override {
    anchored_message_expand_callback_ = std::move(callback);
  }
  void SetAnchoredMessageCollapseCallback(
      base::RepeatingClosure callback) override {
    anchored_message_collapse_callback_ = std::move(callback);
  }
  void SetClickCallback(
      base::RepeatingCallback<void(page_actions::PageActionTrigger)> callback)
      override {
    click_callback_ = std::move(callback);
  }

  // page_actions::PageActionModelObserver:
  void OnPageActionModelChanged(
      const page_actions::PageActionModelInterface& model) override;
  void OnPageActionModelWillBeDeleted(
      const page_actions::PageActionModelInterface& model) override;

  // page_actions::AnchoredMessageBubbleView::Delegate:
  void AnchoredMessageChipClick() override;
  void CloseAnchoredMessage() override;
  void AnchoredMessageExpanded() override;
  void AnchoredMessageCollapsed() override;

  // Returns whether an anchored message bubble is currently visible for this
  // action.
  bool IsAnchoredMessageVisible() const { return anchored_message_ != nullptr; }

  // Returns the AnchoredMessageBubbleView currently showing, or nullptr if none
  // is showing. For testing.
  page_actions::AnchoredMessageBubbleView*
  GetAnchoredMessageForTesting()  // IN-TEST
      const {
    return anchored_message_;
  }

  // Returns the current mojo state for this page action to be sent to WebUI, or
  // nullptr if the action is not visible.
  toolbar_ui_api::mojom::PageActionStatePtr GetState();
  void OnPointerDown();

  // Handles a user click on the page action from WebUI, triggering registered
  // click callbacks and invoking the underlying ActionItem.
  void NotifyClick(PageActionTrigger trigger);

  // Routes a suggestion chip visibility state change notification from WebUI to
  // the controller callback.
  void NotifyChipShowingChanged();

  void SetSuppressionThresholdForTesting(base::TimeDelta threshold) {
    bubble_reopen_suppressor_.SetSuppressionThresholdForTesting(  // IN-TEST
        threshold);
  }

  // Returns the currently observed PageActionModel, or nullptr if not
  // observing.
  const page_actions::PageActionModelInterface* GetObservedModel() const {
    return observation_.IsObserving() ? observation_.GetSource() : nullptr;
  }

  // Returns the active PageActionController for this delegate, or nullptr.
  page_actions::PageActionController* GetController() const {
    return controller_;
  }

 private:
  // Creates and displays the anchored message bubble for `model`. If the bubble
  // is already showing, updates its content. If the anchor element is not yet
  // rendered/tracked in WebUI, registers an ElementTracker callback to defer
  // bubble creation until the element is shown.
  void CreateAndShowAnchoredMessage(
      const page_actions::PageActionModelInterface& model);

  // Callback invoked by ElementTracker when the WebUI anchor element is
  // displayed, creating the deferred anchored message bubble.
  void OnElementShownForAnchoredMessage(ui::TrackedElement* element);

  // Callback invoked when the anchored message bubble widget is closed. Posts
  // tasks to destroy the widget and delegate asynchronously, and notifies the
  // controller if the model still requested an anchored message.
  void OnAnchoredMessageWidgetClose(views::Widget::ClosedReason closed_reason);

  const actions::ActionId action_id_;
  // Safe because the ActionItem tree is owned by BrowserActions (via
  // BrowserWindowFeatures), which is owned by Browser. The delegate is owned
  // by WebUIPageActionControl -> WebUILocationBar -> WebUIToolbarWebView ->
  // ToolbarView -> BrowserView -> BrowserWindow, which is also owned by
  // Browser. Browser destructor destroys BrowserWindow (and thus this delegate)
  // before BrowserWindowFeatures (and thus the ActionItem tree), ensuring
  // action_item_ outlives this delegate.
  const raw_ref<actions::ActionItem> action_item_;
  // Safe because the owner WebUIPageActionControl owns this delegate (via
  // std::unique_ptr in `delegates_` map), so the owner must outlive this
  // delegate.
  const raw_ref<WebUIPageActionControl> owner_;

  // Pointer to the active tab's controller. Updated when the active tab
  // changes. The controller is owned by TabFeatures (which is owned by TabModel
  // which is owned by TabStripModel). We reset/update this pointer in
  // SetController(). If the model is deleted (e.g. tab closed), we reset this
  // to nullptr in OnPageActionModelWillBeDeleted().
  raw_ptr<page_actions::PageActionController> controller_ = nullptr;
  base::ScopedObservation<page_actions::PageActionModelInterface,
                          page_actions::PageActionModelObserver>
      observation_{this};
  base::CallbackListSubscription action_item_subscription_;

  // Callbacks registered by PageActionController via RegisterCallbacks() in
  // SetController(). These are not overridden individually in tests; tests
  // interact with WebUIPageActionControl via PageActionController or public
  // methods on WebUIPageActionControl.
  IsChipShowingChangedCallback is_chip_showing_changed_callback_ =
      base::DoNothing();
  ImageAnimationStartedCallback image_animation_started_callback_ =
      base::DoNothing();
  base::RepeatingClosure anchored_message_close_callback_ = base::DoNothing();
  base::RepeatingClosure anchored_message_expand_callback_ = base::DoNothing();
  base::RepeatingClosure anchored_message_collapse_callback_ =
      base::DoNothing();
  base::RepeatingCallback<void(page_actions::PageActionTrigger)>
      click_callback_ = base::DoNothing();

  // The last state sent to the WebUI. Null if the action was not visible.
  toolbar_ui_api::mojom::PageActionStatePtr old_state_;
  bool was_chip_visible_ = false;
  bool was_showing_bubble_ = false;

  WebUIBubbleReopenSuppressor bubble_reopen_suppressor_;

  toolbar_ui_api::IconHandle cached_icon_;

  // The AnchoredMessageBubbleView is owned and destroyed by the
  // DialogClientView of `anchored_message_widget_`.
  raw_ptr<page_actions::AnchoredMessageBubbleView> anchored_message_ = nullptr;
  std::unique_ptr<views::Widget> anchored_message_widget_;
  base::CallbackListSubscription element_shown_subscription_;
  base::WeakPtrFactory<WebUIPageActionDelegate> weak_factory_{this};
};

WebUIPageActionControl::WebUIPageActionDelegate::~WebUIPageActionDelegate() {
  if (anchored_message_widget_) {
    anchored_message_ = nullptr;
    anchored_message_widget_.reset();
  }
  element_shown_subscription_ = {};
  observation_.Reset();
}

void WebUIPageActionControl::WebUIPageActionDelegate::SetController(
    page_actions::PageActionController* controller) {
  if (observation_.IsObserving() &&
      observation_.GetSource()->ShouldShowAnchoredMessage()) {
    CloseAnchoredMessage();
  }
  if (anchored_message_widget_) {
    anchored_message_ = nullptr;
    anchored_message_widget_.reset();
  }
  element_shown_subscription_ = {};
  observation_.Reset();
  action_item_subscription_ = {};
  controller_ = controller;
  was_chip_visible_ = false;
  was_showing_bubble_ = false;

  if (controller_) {
    controller_->RegisterCallbacks(page_actions::PageActionPassKey(),
                                   action_id_, this);
    controller_->AddObserver(action_id_, observation_);
    action_item_subscription_ =
        controller_->CreateActionItemSubscription(&*action_item_);
    OnPageActionModelChanged(*observation_.GetSource());
  } else {
    is_chip_showing_changed_callback_ = base::DoNothing();
    image_animation_started_callback_ = base::DoNothing();
    anchored_message_close_callback_ = base::DoNothing();
    anchored_message_expand_callback_ = base::DoNothing();
    anchored_message_collapse_callback_ = base::DoNothing();
    click_callback_ = base::DoNothing();
    if (old_state_) {
      old_state_ = nullptr;
      owner_->NotifyPageActionStateChanged();
    }
  }
}

void WebUIPageActionControl::WebUIPageActionDelegate::UpdateStateAndNotify() {
  if (observation_.IsObserving()) {
    OnPageActionModelChanged(*observation_.GetSource());
  }
}

void WebUIPageActionControl::WebUIPageActionDelegate::OnPageActionModelChanged(
    const page_actions::PageActionModelInterface& model) {
  const bool visible = model.GetVisible();
  const bool is_showing_bubble = model.GetActionItemIsShowingBubble();
  if (was_showing_bubble_ && !is_showing_bubble) {
    bubble_reopen_suppressor_.RecordBubbleClosed();
  }
  was_showing_bubble_ = is_showing_bubble;

  const bool is_chip_visible = visible && model.ShouldShowSuggestionChip();

  if (model.GetShouldAnnounceChip() && !was_chip_visible_ && is_chip_visible) {
    owner_->AnnounceAlert(model.GetText());
  }
  was_chip_visible_ = is_chip_visible;

  toolbar_ui_api::mojom::PageActionStatePtr new_state = GetState();

  if (!old_state_.Equals(new_state)) {
    old_state_ = std::move(new_state);
    owner_->NotifyPageActionStateChanged();
  }

  if (visible && model.ShouldShowAnchoredMessage()) {
    CreateAndShowAnchoredMessage(model);
  } else {
    // We want to hide the message. First, unconditionally clear any pending
    // subscriptions waiting to spawn the bubble.
    element_shown_subscription_ = {};

    // Then, close the active bubble widget if it is currently open.
    if (anchored_message_widget_ && !anchored_message_widget_->IsClosed()) {
      anchored_message_widget_->CloseWithReason(
          views::Widget::ClosedReason::kUnspecified);
    }
  }
}

void WebUIPageActionControl::WebUIPageActionDelegate::
    OnPageActionModelWillBeDeleted(
        const page_actions::PageActionModelInterface& model) {
  if (anchored_message_widget_ && !anchored_message_widget_->IsClosed()) {
    anchored_message_widget_->CloseWithReason(
        views::Widget::ClosedReason::kUnspecified);
  }
  element_shown_subscription_ = {};
  observation_.Reset();
  action_item_subscription_ = {};
  controller_ = nullptr;
  was_chip_visible_ = false;
  was_showing_bubble_ = false;
  if (old_state_) {
    old_state_ = nullptr;
    owner_->NotifyPageActionStateChanged();
  }
}

toolbar_ui_api::mojom::PageActionStatePtr
WebUIPageActionControl::WebUIPageActionDelegate::GetState() {
  if (!observation_.IsObserving()) {
    return nullptr;
  }
  const auto* model = observation_.GetSource();
  if (!model->GetVisible()) {
    return nullptr;
  }

  auto state = toolbar_ui_api::mojom::PageActionState::New();
  state->page_action_id =
      webui_toolbar::ActionIdToMojomPageActionId(action_id_);
  state->accessible_name = model->GetAccessibleName();
  state->tooltip_text = model->GetTooltipText();
  if (model->ShouldShowAnchoredMessage() && !state->tooltip_text.empty()) {
    state->tooltip_text = l10n_util::GetStringFUTF16(
        IDS_PAGE_ACTION_ANCHORED_MESSAGE_SHOWING, state->tooltip_text);
  }
  ui::ImageModel image_model = model->GetImage();

  // `view` may be null in unit tests.
  auto* view = owner_->webui_delegate_->GetView();
  const ui::ColorProvider* color_provider =
      view ? view->GetColorProvider() : nullptr;
  if (model->GetColorSource() ==
          page_actions::PageActionColorSource::kCascadingAccent &&
      image_model.IsVectorIcon()) {
    const auto& vector_icon_model = image_model.GetVectorIcon();
    const SkColor default_color =
        color_provider->GetColor(ui::kColorFocusableBorderFocused);
    // Page actions are displayed on the toolbar, so `kColorToolbar` is used as
    // the background color for contrast calculations (matching what
    // `views::GetCascadingBackgroundColor()` resolves in native Views via
    // `ToolbarView`).
    const SkColor background_color = color_provider->GetColor(kColorToolbar);
    const SkColor blended_color =
        color_utils::BlendForMinContrast(
            default_color, background_color, std::nullopt,
            color_utils::kMinimumVisibleContrastRatio)
            .color;
    image_model = ui::ImageModel::FromVectorIcon(
        *vector_icon_model.vector_icon(), blended_color,
        vector_icon_model.icon_size(), vector_icon_model.badge_icon());
  }
  state->icon = cached_icon_ =
      owner_->webui_delegate_->GetIconTable().RegisterImageModelTryReuse(
          image_model, cached_icon_);
  state->text = model->GetText();
  state->should_show_chip = model->ShouldShowSuggestionChip();
  state->should_animate_chip_in = model->GetShouldAnimateChipIn();
  state->should_animate_chip_out = model->GetShouldAnimateChipOut();

  std::optional<ui::ColorId> override_color_id =
      model->GetOverrideBackgroundColorId();
  if (override_color_id.has_value()) {
    state->background_color_override =
        color_provider->GetColor(*override_color_id);
  }
  std::string identifier_name;
  page_actions::PageActionPropertiesProvider provider;
  if (provider.Contains(action_id_)) {
    ui::ElementIdentifier element_id =
        provider.GetProperties(action_id_).element_identifier;
    if (element_id) {
      identifier_name = element_id.GetName();
    }
  }
  state->identifier = tracked_element::mojom::TrackedElementIdentifier::New(
      std::move(identifier_name),
      /*secondary_identifier=*/std::string());
  state->is_active = model->GetActionActive();

  // Pass the current icon animation token to the WebUI so it can detect tab
  // switches and suppress animations.
  state->icon_animation_token = owner_->icon_animation_token_;
  return state;
}

void WebUIPageActionControl::WebUIPageActionDelegate::OnPointerDown() {
  const bool is_showing_bubble =
      observation_.IsObserving() &&
      observation_.GetSource()->GetActionItemIsShowingBubble();
  bubble_reopen_suppressor_.OnMousePressed(is_showing_bubble);
}

void WebUIPageActionControl::WebUIPageActionDelegate::NotifyClick(
    PageActionTrigger trigger) {
  // Ignore clicks received during shutdown.
  if (!observation_.IsObserving() || !observation_.GetSource()->GetVisible()) {
    return;
  }

  const bool is_pointer_interaction = (trigger == PageActionTrigger::kMouse ||
                                       trigger == PageActionTrigger::kGesture);
  if (bubble_reopen_suppressor_.ShouldSuppressBubbleShow(
          is_pointer_interaction)) {
    return;
  }

  click_callback_.Run(trigger);

  auto builder =
      actions::ActionInvocationContext::Builder()
          .SetProperty(page_actions::kPageActionTriggerKey, trigger)
          .SetProperty(page_actions::kPageActionEntryPointKey,
                       page_actions::PageActionEntryPoint::kSuggestionChip);
  if (auto side_panel_trigger =
          GetSidePanelOpenTriggerForPageAction(action_item_->GetActionId())) {
    builder = std::move(builder).SetProperty(kSidePanelOpenTriggerKey,
                                             *side_panel_trigger);
  }
  action_item_->InvokeAction(std::move(builder).Build());
}

void WebUIPageActionControl::WebUIPageActionDelegate::
    NotifyChipShowingChanged() {
  if (observation_.IsObserving()) {
    bool is_chip_showing = observation_.GetSource()->ShouldShowSuggestionChip();
    is_chip_showing_changed_callback_.Run(is_chip_showing);
  }
}

void WebUIPageActionControl::WebUIPageActionDelegate::
    AnchoredMessageChipClick() {
  click_callback_.Run(page_actions::PageActionTrigger::kMouse);
  auto builder =
      actions::ActionInvocationContext::Builder()
          .SetProperty(page_actions::kPageActionTriggerKey,
                       page_actions::PageActionTrigger::kMouse)
          .SetProperty(page_actions::kPageActionEntryPointKey,
                       page_actions::PageActionEntryPoint::kAnchoredMessage);
  if (auto side_panel_trigger =
          GetSidePanelOpenTriggerForPageAction(action_item_->GetActionId())) {
    builder = std::move(builder).SetProperty(kSidePanelOpenTriggerKey,
                                             *side_panel_trigger);
  }
  action_item_->InvokeAction(std::move(builder).Build());
  anchored_message_close_callback_.Run();
}

void WebUIPageActionControl::WebUIPageActionDelegate::CloseAnchoredMessage() {
  anchored_message_close_callback_.Run();
}

void WebUIPageActionControl::WebUIPageActionDelegate::
    AnchoredMessageExpanded() {
  anchored_message_expand_callback_.Run();
}

void WebUIPageActionControl::WebUIPageActionDelegate::
    AnchoredMessageCollapsed() {
  anchored_message_collapse_callback_.Run();
}

void WebUIPageActionControl::WebUIPageActionDelegate::
    CreateAndShowAnchoredMessage(
        const page_actions::PageActionModelInterface& model) {
  if (anchored_message_) {
    anchored_message_->UpdateContent(model);
    return;
  }

  page_actions::PageActionViewInterface* view_interface =
      owner_->GetPageActionViewInterface(action_id_);
  if (!view_interface) {
    return;
  }
  views::BubbleAnchor anchor = view_interface->GetBubbleAnchor();
  if (anchor.IsNull()) {
    // In WebUI toolbar, the UI renders asynchronously in the WebContents, so
    // the page action element (e.g. the toolbar-chip-button) may not have been
    // painted or registered with ElementTracker yet when the anchored message
    // is requested. In that case, subscribe to ElementTracker to wait for the
    // element to be shown before creating and anchoring the bubble.
    page_actions::PageActionPropertiesProvider provider;
    if (provider.Contains(action_id_)) {
      ui::ElementIdentifier element_id =
          provider.GetProperties(action_id_).element_identifier;
      BrowserWindowInterface* browser = owner_->GetBrowser();
      if (element_id && browser) {
        element_shown_subscription_ =
            ui::ElementTracker::GetElementTracker()->AddElementShownCallback(
                element_id, BrowserElements::From(browser)->GetContext(),
                base::BindRepeating(
                    &WebUIPageActionControl::WebUIPageActionDelegate::
                        OnElementShownForAnchoredMessage,
                    weak_factory_.GetWeakPtr()));
      }
    }
    return;
  }

  auto message_delegate =
      std::make_unique<page_actions::AnchoredMessageBubbleView>(anchor, model,
                                                                *this);
  anchored_message_ = message_delegate.get();

  // AnchoredMessageBubbleView inherits from both BubbleDialogDelegate and View,
  // returning `this` from `GetContentsView()`. During CreateBubble's Widget
  // initialization, DialogClientView adds the contents view to the View
  // hierarchy (via ClientView::ViewHierarchyChanged -> AddChildViewAt), taking
  // ownership of the View. When the Widget is destroyed, DialogClientView is
  // destroyed and deletes its child contents view (the
  // AnchoredMessageBubbleView instance). Therefore, releasing
  // `message_delegate` here does not leak memory.
  anchored_message_widget_ = views::BubbleDialogDelegate::CreateBubble(
      message_delegate.release(),
      base::BindOnce(&WebUIPageActionControl::WebUIPageActionDelegate::
                         OnAnchoredMessageWidgetClose,
                     weak_factory_.GetWeakPtr()));

  if (anchored_message_widget_) {
    // Don't steal focus when shown.
    anchored_message_widget_->ShowInactive();
  } else {
    anchored_message_ = nullptr;
  }
}

void WebUIPageActionControl::WebUIPageActionDelegate::
    OnElementShownForAnchoredMessage(ui::TrackedElement* element) {
  element_shown_subscription_ = {};
  if (observation_.IsObserving() &&
      observation_.GetSource()->ShouldShowAnchoredMessage() &&
      observation_.GetSource()->GetVisible()) {
    CreateAndShowAnchoredMessage(*observation_.GetSource());
  }
}

void WebUIPageActionControl::WebUIPageActionDelegate::
    OnAnchoredMessageWidgetClose(views::Widget::ClosedReason closed_reason) {
  if (!anchored_message_) {
    return;
  }
  CHECK(anchored_message_widget_);
  anchored_message_ = nullptr;
  if (anchored_message_widget_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
        FROM_HERE, std::move(anchored_message_widget_));
  }

  if (observation_.IsObserving() &&
      observation_.GetSource()->ShouldShowAnchoredMessage()) {
    CloseAnchoredMessage();
  }
}

WebUIPageActionControl::WebUIPageActionControl(
    actions::ActionItem* root_action_item)
    : root_action_item_(root_action_item) {
  for (actions::ActionId action_id : page_actions::kActionIds) {
    actions::ActionItem* action_item = nullptr;
    if (root_action_item) {
      action_item =
          actions::ActionManager::Get().FindAction(action_id, root_action_item);
    }
    if (action_item) {
      delegates_[action_id] = std::make_unique<WebUIPageActionDelegate>(
          action_id, *action_item, *this);
      views_[action_id] =
          std::make_unique<WebUIPageActionView>(action_id, *this);
    }
  }
}

WebUIPageActionControl::~WebUIPageActionControl() = default;

void WebUIPageActionControl::Init(WebUIToolbarControlDelegate* webui_delegate) {
  webui_delegate_ = webui_delegate;
}

PageActionViewInterface* WebUIPageActionControl::GetPageActionViewInterface(
    actions::ActionId action_id) {
  auto it = views_.find(action_id);
  if (it != views_.end()) {
    return it->second.get();
  }
  return nullptr;
}

BrowserWindowInterface* WebUIPageActionControl::GetBrowser() {
  return webui_delegate_ ? webui_delegate_->GetBrowser() : nullptr;
}

const page_actions::PageActionModelInterface*
WebUIPageActionControl::GetObservedModel(actions::ActionId action_id) const {
  auto it = delegates_.find(action_id);
  if (it != delegates_.end()) {
    return it->second->GetObservedModel();
  }
  return nullptr;
}

page_actions::PageActionController* WebUIPageActionControl::GetController(
    actions::ActionId action_id) {
  auto it = delegates_.find(action_id);
  if (it != delegates_.end()) {
    return it->second->GetController();
  }
  return nullptr;
}

void WebUIPageActionControl::UpdateController(
    content::WebContents* web_contents) {
  page_actions::PageActionController* new_controller = nullptr;
  if (web_contents) {
    tabs::TabInterface* tab =
        tabs::TabInterface::MaybeGetFromContents(web_contents);
    if (tab) {
      new_controller = page_actions::PageActionController::From(tab);
    }
  }

  const bool tab_changed = active_controller_ != new_controller;

  if (tab_changed) {
    active_controller_subscription_ = {};
    active_controller_ = new_controller;

    if (active_controller_) {
      active_controller_subscription_ =
          active_controller_->RegisterOnWillDestroyCallback(
              base::BindOnce(&WebUIPageActionControl::OnControllerDestroying,
                             base::Unretained(this)));
    }

    if (features::IsToolbarGlowUpBookmarkEnabled()) {
      // Increment the icon animation token to signal to the WebUI that the
      // active tab has changed. This allows the WebUI to suppress transition
      // icon animations on tab switches.
      ++icon_animation_token_;
      last_url_spec_ =
          web_contents ? web_contents->GetLastCommittedURL().spec() : "";
    }

    // Only re-initialize delegates if the tab (and controller) actually
    // changed.
    for (auto& [action_id, delegate] : delegates_) {
      delegate->SetController(active_controller_);
    }
    return;
  }

  // Same-tab navigation: only handle animation suppression when bookmark glow
  // up is enabled.
  if (features::IsToolbarGlowUpBookmarkEnabled() && web_contents) {
    const std::string current_url = web_contents->GetLastCommittedURL().spec();
    if (current_url != last_url_spec_) {
      last_url_spec_ = current_url;
      // Increment the icon animation token and notify WebUI immediately on
      // navigation so that page-load icon changes are suppressed while
      // subsequent in-page user interactions (e.g. starring/unstarring via
      // the bubble) can animate.
      ++icon_animation_token_;
      for (auto& [action_id, delegate] : delegates_) {
        delegate->UpdateStateAndNotify();
      }
    }
  }
}

void WebUIPageActionControl::SetShouldHidePageActions(
    bool should_hide_page_actions) {
  if (active_controller_) {
    active_controller_->SetShouldHidePageActions(should_hide_page_actions);
  }
}

std::vector<toolbar_ui_api::mojom::PageActionStatePtr>
WebUIPageActionControl::GetPageActionStates() {
  std::vector<toolbar_ui_api::mojom::PageActionStatePtr> states;
  for (actions::ActionId action_id : page_actions::kActionIds) {
    auto it = delegates_.find(action_id);
    if (it != delegates_.end()) {
      auto state = it->second->GetState();
      if (state) {
        states.push_back(std::move(state));
      }
    }
  }
  return states;
}

void WebUIPageActionControl::OnPageActionPointerDown(
    toolbar_ui_api::mojom::PageActionId action_id) {
  auto it =
      delegates_.find(webui_toolbar::MojomPageActionIdToActionId(action_id));
  if (it != delegates_.end()) {
    it->second->OnPointerDown();
  }
}

void WebUIPageActionControl::OnPageActionClick(
    toolbar_ui_api::mojom::PageActionId action_id,
    PageActionTrigger trigger,
    toolbar_ui_api::mojom::ToolbarUIService::OnPageActionClickCallback
        callback) {
  auto it =
      delegates_.find(webui_toolbar::MojomPageActionIdToActionId(action_id));
  if (it == delegates_.end()) {
    std::move(callback).Run(base::unexpected(
        Error::New(Code::kInvalidArgument, "Invalid PageActionId")));
    return;
  }

  it->second->NotifyClick(trigger);
  std::move(callback).Run(std::monostate());
}

void WebUIPageActionControl::SetSuppressionThresholdForTesting(
    base::TimeDelta threshold) {
  for (auto& [action_id, delegate] : delegates_) {
    delegate->SetSuppressionThresholdForTesting(threshold);  // IN-TEST
  }
}

void WebUIPageActionControl::OnPageActionChipShowingChanged(
    toolbar_ui_api::mojom::PageActionId action_id,
    toolbar_ui_api::mojom::ToolbarUIService::
        OnPageActionChipShowingChangedCallback callback) {
  auto it =
      delegates_.find(webui_toolbar::MojomPageActionIdToActionId(action_id));
  if (it == delegates_.end()) {
    std::move(callback).Run(base::unexpected(
        Error::New(Code::kInvalidArgument, "Invalid PageActionId")));
    return;
  }

  it->second->NotifyChipShowingChanged();
  std::move(callback).Run(std::monostate());
}

bool WebUIPageActionControl::IsAnchoredMessageShowing(
    actions::ActionId action_id) const {
  auto it = delegates_.find(action_id);
  if (it != delegates_.end()) {
    return it->second->IsAnchoredMessageVisible();
  }
  return false;
}

page_actions::AnchoredMessageBubbleView*
WebUIPageActionControl::GetAnchoredMessageForTesting(  // IN-TEST
    actions::ActionId action_id) {
  auto it = delegates_.find(action_id);
  if (it != delegates_.end()) {
    return it->second->GetAnchoredMessageForTesting();  // IN-TEST
  }
  return nullptr;
}

void WebUIPageActionControl::NotifyPageActionStateChanged() {
  if (webui_delegate_) {
    webui_delegate_->OnPageActionChanged(GetPageActionStates());
  }
}

void WebUIPageActionControl::AnnounceAlert(const std::u16string& announcement) {
  if (webui_delegate_) {
    webui_delegate_->AnnounceAlert(announcement);
  }
}

void WebUIPageActionControl::OnControllerDestroying(
    page_actions::PageActionController& controller) {
  CHECK_EQ(active_controller_, &controller);
  UpdateController(nullptr);
}

}  // namespace page_actions
