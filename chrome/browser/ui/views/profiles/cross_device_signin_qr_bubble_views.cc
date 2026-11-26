// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/cancelable_callback.h"
#include "base/check.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/signin/cross_device_signin_qr_bubble.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "chrome/browser/ui/views/bubble_anchor_util_views.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/toolbar/avatar_toolbar_button_interface.h"
#include "chrome/browser/ui/webui/signin/cross_device_signin_qr_bubble_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_change_event.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/actions/actions.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

DEFINE_ELEMENT_IDENTIFIER_VALUE(kCrossDeviceSigninQrBubbleWebViewElementId);

namespace {

constexpr int kDialogWidth = 370;
constexpr int kHorizontalPadding = 40;
constexpr int kWebViewWidth = kDialogWidth - kHorizontalPadding;

// Post-save promos anchor the QR code bubble to the omnibox page action of the
// saved data type, and close it when it loses focus.
struct OmniboxAnchorConfig {
  actions::ActionId action_id;
  ui::ElementIdentifier highlighted_element;
};

constexpr std::optional<OmniboxAnchorConfig> GetOmniboxAnchorConfig(
    CrossDeviceSigninPromoEntryPoint entry_point) {
  switch (entry_point) {
    case CrossDeviceSigninPromoEntryPoint::kProfileMenu:
    case CrossDeviceSigninPromoEntryPoint::kHistoryPage:
    case CrossDeviceSigninPromoEntryPoint::kSendTabToSelf:
    case CrossDeviceSigninPromoEntryPoint::kWhatsNewPage:
    case CrossDeviceSigninPromoEntryPoint::kForeignTabs:
      return std::nullopt;
    case CrossDeviceSigninPromoEntryPoint::kBookmarkSave:
      return OmniboxAnchorConfig{
          .action_id = kActionBookmarkThisTab,
          .highlighted_element = kBookmarkStarViewElementId};
    case CrossDeviceSigninPromoEntryPoint::kPasswordSave:
      return OmniboxAnchorConfig{
          .action_id = kActionShowPasswordsBubbleOrPage,
          .highlighted_element = kPasswordsOmniboxKeyIconElementId};
  }
}

// Anchor delegate for standard entry points anchored to the avatar button.
class AvatarQrBubbleAnchor : public CrossDeviceSigninQrBubbleAnchor {
 public:
  explicit AvatarQrBubbleAnchor(BrowserWindowInterface* browser)
      : browser_(browser->GetWeakPtr()) {}

  ~AvatarQrBubbleAnchor() override { ReleaseState(); }

  void ResolveAnchor(base::OnceClosure on_resolved) override {
    if (AvatarToolbarButtonInterface* avatar_button = GetAvatarButton()) {
      resolve_callback_.Reset(base::BindOnce(&AvatarQrBubbleAnchor::OnVisible,
                                             base::Unretained(this),
                                             std::move(on_resolved)));
      avatar_button->RunWhenVisible(resolve_callback_.callback());
      return;
    }
    std::move(on_resolved).Run();
  }

  views::BubbleAnchor GetBubbleAnchor() const override { return anchor_; }

  bool ShouldCloseOnDeactivate() const override { return false; }

  void ConfigureBubble(views::BubbleDialogModelHost& bubble,
                       views::BubbleAnchor anchor) override {
    if (AvatarToolbarButtonInterface* avatar_button = GetAvatarButton()) {
      clear_avatar_button_effects_ = avatar_button->SetExplicitButtonState(
          l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SIGNIN_ON_PHONE),
          /*accessibility_label=*/std::nullopt,
          /*explicit_action=*/
          base::BindRepeating(
              [](base::WeakPtr<BrowserWindowInterface> weak_browser,
                 bool is_source_accelerator) {
                if (weak_browser) {
                  SigninViewController::From(weak_browser.get())
                      ->CloseBubbleSignin();
                }
              },
              browser_));
    }
  }

  bool OnBeforeShow() override { return true; }

  void ReleaseState() override {
    resolve_callback_.Cancel();
    clear_avatar_button_effects_.RunAndReset();
    keep_visible_.RunAndReset();
  }

 private:
  AvatarToolbarButtonInterface* GetAvatarButton() const {
    BrowserView* browser_view =
        browser_ ? BrowserView::GetBrowserViewForBrowser(browser_.get())
                 : nullptr;
    ToolbarButtonProvider* button_provider =
        browser_view ? browser_view->toolbar_button_provider() : nullptr;
    return button_provider ? button_provider->GetAvatarToolbarButtonInterface()
                           : nullptr;
  }

  void OnVisible(base::OnceClosure on_resolved,
                 views::BubbleAnchor anchor,
                 base::ScopedClosureRunner keep_visible) {
    anchor_ = anchor;
    keep_visible_ = std::move(keep_visible);
    std::move(on_resolved).Run();
  }

  base::WeakPtr<BrowserWindowInterface> browser_;
  views::BubbleAnchor anchor_;
  base::ScopedClosureRunner keep_visible_;
  base::ScopedClosureRunner clear_avatar_button_effects_;
  base::CancelableOnceCallback<void(views::BubbleAnchor,
                                    base::ScopedClosureRunner)>
      resolve_callback_;
};

// Anchor delegate for post-save promos anchored to an omnibox page action.
class OmniboxPageActionQrBubbleAnchor : public CrossDeviceSigninQrBubbleAnchor {
 public:
  OmniboxPageActionQrBubbleAnchor(BrowserWindowInterface* browser,
                                  OmniboxAnchorConfig config)
      : browser_(browser->GetWeakPtr()), config_(config) {}

  ~OmniboxPageActionQrBubbleAnchor() override { ReleaseState(); }

  void ResolveAnchor(base::OnceClosure on_resolved) override {
    std::move(on_resolved).Run();
  }

  views::BubbleAnchor GetBubbleAnchor() const override {
    BrowserView* browser_view =
        browser_ ? BrowserView::GetBrowserViewForBrowser(browser_.get())
                 : nullptr;
    ToolbarButtonProvider* button_provider =
        browser_view ? browser_view->toolbar_button_provider() : nullptr;
    return button_provider ? button_provider->GetBubbleAnchor(config_.action_id)
                           : views::BubbleAnchor();
  }

  bool ShouldCloseOnDeactivate() const override { return true; }

  void ConfigureBubble(views::BubbleDialogModelHost& bubble,
                       views::BubbleAnchor anchor) override {
    BrowserView* browser_view =
        browser_ ? BrowserView::GetBrowserViewForBrowser(browser_.get())
                 : nullptr;
    if (browser_view && config_.highlighted_element &&
        !bubble_anchor_util::IsHighlightable(anchor)) {
      bubble.SetHighlightedElement(config_.highlighted_element);
    }
  }

  void AttachToBubble(base::OnceClosure close_bubble) override {
    close_bubble_ = std::move(close_bubble);
    // The anchor belongs to the active tab's omnibox: close the bubble when
    // that tab is no longer active.
    if (tabs::TabInterface* tab =
            browser_ ? browser_->GetActiveTabInterface() : nullptr) {
      tab_subscriptions_.push_back(tab->RegisterWillDeactivate(
          base::BindRepeating(&OmniboxPageActionQrBubbleAnchor::OnTabDeactivate,
                              base::Unretained(this))));
      tab_subscriptions_.push_back(tab->RegisterWillDetach(
          base::BindRepeating(&OmniboxPageActionQrBubbleAnchor::OnTabDetach,
                              base::Unretained(this))));
    }
  }

  bool OnBeforeShow() override {
    // Another bubble (e.g. `BookmarkBubbleView`) may have claimed the omnibox
    // page action while the WebUI was still loading.
    actions::ActionItem* action_item = GetActionItem();
    if (action_item && action_item->GetIsShowingBubble()) {
      return false;
    }
    // Marks the omnibox page action as showing a bubble, like its own bubble
    // does, so that clicking it closes this bubble without reopening its own.
    if (!is_showing_bubble_set_ && action_item) {
      action_item->SetIsShowingBubble(true);
      is_showing_bubble_set_ = true;
    }
    return true;
  }

  // Releases `ActionItem::GetIsShowingBubble()` synchronously on non-focus-loss
  // closes so a replacement bubble opened in the same task is not clobbered
  // when `DeleteSoon()` later destroys this widget. For `kLostFocus`, cleanup
  // remains deferred until `DeleteSoon()` runs so that clicking the omnibox
  // page action icon still sees `GetIsShowingBubble() == true` and suppresses
  // reopening its own bubble on release.
  void OnWidgetClosing(views::Widget::ClosedReason reason) override {
    tab_subscriptions_.clear();
    if (reason != views::Widget::ClosedReason::kLostFocus) {
      ReleaseState();
    }
  }

  void ReleaseState() override {
    tab_subscriptions_.clear();
    close_bubble_.Reset();
    if (std::exchange(is_showing_bubble_set_, false)) {
      if (actions::ActionItem* action_item = GetActionItem()) {
        action_item->SetIsShowingBubble(false);
      }
    }
  }

 private:
  actions::ActionItem* GetActionItem() const {
    if (!browser_) {
      return nullptr;
    }
    BrowserActions* browser_actions = BrowserActions::From(browser_.get());
    if (!browser_actions || !browser_actions->root_action_item()) {
      return nullptr;
    }
    return actions::ActionManager::Get().FindAction(
        config_.action_id, browser_actions->root_action_item());
  }

  void CloseBubble() {
    if (base::OnceClosure close = std::move(close_bubble_)) {
      std::move(close).Run();
    }
  }

  void OnTabDeactivate(tabs::TabInterface* tab) { CloseBubble(); }

  void OnTabDetach(tabs::TabInterface* tab,
                   tabs::TabInterface::DetachReason reason) {
    CloseBubble();
  }

  base::WeakPtr<BrowserWindowInterface> browser_;
  const OmniboxAnchorConfig config_;
  bool is_showing_bubble_set_ = false;
  base::OnceClosure close_bubble_;
  std::vector<base::CallbackListSubscription> tab_subscriptions_;
};

class CrossDeviceSigninQrWebView : public views::WebView,
                                   public signin::IdentityManager::Observer {
 public:
  CrossDeviceSigninQrWebView(
      BrowserWindowInterface* browser,
      std::unique_ptr<CrossDeviceSigninQrBubbleAnchor> anchor)
      : views::WebView(browser->GetProfile()),
        profile_(browser->GetProfile()),
        browser_(browser->GetWeakPtr()),
        anchor_(std::move(anchor)) {
    CHECK(anchor_);
    if (signin::IdentityManager* identity_manager =
            IdentityManagerFactory::GetForProfile(profile_)) {
      identity_manager_observation_.Observe(identity_manager);
    }
    anchor_->AttachToBubble(
        base::BindOnce(&CrossDeviceSigninQrWebView::CloseWidget,
                       weak_ptr_factory_.GetWeakPtr()));
    SetAcceleratorHandling(views::WebContentsAcceleratorHandling::kEnabled);
  }

  // Also releases anchor state if the view is destroyed without the bubble's
  // dialog destroying callback having run. No-op if already released.
  ~CrossDeviceSigninQrWebView() override { ReleaseAnchorState(); }

  base::WeakPtr<CrossDeviceSigninQrWebView> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void ConfigureBubble(views::BubbleDialogModelHost& bubble,
                       views::BubbleAnchor anchor) {
    anchor_->ConfigureBubble(bubble, anchor);
  }

  void OnWidgetClosingSynchronously(views::Widget::ClosedReason reason) {
    is_closing_ = true;
    anchor_->OnWidgetClosing(reason);
  }

  void ReleaseAnchorState() { anchor_->ReleaseState(); }

  // content::WebContentsDelegate:
  bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                         const content::ContextMenuParams& params) override {
    // Suppresses the context menu because some features, such as inspecting
    // elements, are not appropriate in a bubble.
    return true;
  }

  void ResizeDueToAutoResize(content::WebContents* source,
                             const gfx::Size& new_size) override {
    views::WebView::ResizeDueToAutoResize(source, new_size);
    views::Widget* widget = GetWidget();
    if (!widget) {
      return;
    }
    if (auto* bubble_delegate =
            widget->widget_delegate()->AsBubbleDialogDelegate()) {
      bubble_delegate->SizeToContents();
    }

    ShowWidgetIfNeeded();
  }

  // signin::IdentityManager::Observer:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override {
    if (event_details.GetEventTypeFor(signin::ConsentLevel::kSignin) ==
        signin::PrimaryAccountChangeEvent::Type::kCleared) {
      CloseWidget();
    }
  }

  void OnRefreshTokenRemovedForAccount(
      const CoreAccountId& account_id) override {
    if (IsPrimaryAccount(account_id)) {
      CloseWidget();
    }
  }

  void OnErrorStateOfRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info,
      const GoogleServiceAuthError& error,
      signin_metrics::SourceForRefreshTokenOperation token_operation_source)
      override {
    if (error.IsPersistentError() &&
        IsPrimaryAccount(account_info.account_id)) {
      CloseWidget();
    }
  }

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override {
    if (auto* view = GetWebContents()->GetRenderWidgetHostView()) {
      view->EnableAutoResize(gfx::Size(kWebViewWidth, 100),
                             gfx::Size(kWebViewWidth, 800));
    }
  }

  void DidStopLoading() override {
    // Fallback: If auto-resize didn't fire (e.g. because the size matched
    // the placeholder or due to Wayland hidden state issues), ensure the
    // widget is shown to avoid deadlocks.
    ShowWidgetIfNeeded();
  }

 private:
  void ShowWidgetIfNeeded() {
    views::Widget* widget = GetWidget();
    // The widget may have started closing (e.g. via `CloseBubbleSignin()`,
    // tab deactivation, or sign-out) while the WebUI was still loading;
    // `MakeCloseSynchronous()` defers destruction via `DeleteSoon()` without
    // setting `Widget::IsClosed()`.
    if (is_closing_ || !widget || widget->IsClosed() || widget->IsVisible()) {
      return;
    }
    SigninViewController* controller =
        browser_ ? SigninViewController::From(browser_.get()) : nullptr;
    if (!controller || !controller->IsShowingBubbleWidget(widget)) {
      return;
    }
    if (!anchor_->OnBeforeShow()) {
      CloseWidget();
      return;
    }
    widget->Show();
  }

  void CloseWidget() {
    const bool was_closing = std::exchange(is_closing_, true);
    anchor_->ReleaseState();
    if (!was_closing) {
      if (views::Widget* widget = GetWidget()) {
        widget->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
      }
    }
  }

  bool IsPrimaryAccount(const CoreAccountId& account_id) const {
    auto* identity_manager = IdentityManagerFactory::GetForProfile(profile_);
    return identity_manager && identity_manager->GetPrimaryAccountId(
                                   signin::ConsentLevel::kSignin) == account_id;
  }

  raw_ptr<Profile> profile_;
  base::WeakPtr<BrowserWindowInterface> browser_;
  std::unique_ptr<CrossDeviceSigninQrBubbleAnchor> anchor_;
  bool is_closing_ = false;
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};
  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;
  base::WeakPtrFactory<CrossDeviceSigninQrWebView> weak_ptr_factory_{this};
};

}  // namespace

CrossDeviceSigninQrBubble::CrossDeviceSigninQrBubble(
    std::unique_ptr<views::BubbleDialogDelegate> delegate,
    base::OnceCallback<void(views::Widget::ClosedReason)> on_widget_closing)
    : delegate(std::move(delegate)),
      on_widget_closing(std::move(on_widget_closing)) {}

CrossDeviceSigninQrBubble::CrossDeviceSigninQrBubble(
    CrossDeviceSigninQrBubble&&) noexcept = default;
CrossDeviceSigninQrBubble& CrossDeviceSigninQrBubble::operator=(
    CrossDeviceSigninQrBubble&&) noexcept = default;
CrossDeviceSigninQrBubble::~CrossDeviceSigninQrBubble() = default;

std::unique_ptr<CrossDeviceSigninQrBubbleAnchor>
CreateCrossDeviceSigninQrBubbleAnchor(
    BrowserWindowInterface* browser,
    CrossDeviceSigninPromoEntryPoint entry_point) {
  if (std::optional<OmniboxAnchorConfig> omnibox_config =
          GetOmniboxAnchorConfig(entry_point)) {
    return std::make_unique<OmniboxPageActionQrBubbleAnchor>(browser,
                                                             *omnibox_config);
  }
  return std::make_unique<AvatarQrBubbleAnchor>(browser);
}

CrossDeviceSigninQrBubble CreateCrossDeviceSigninQrBubble(
    BrowserWindowInterface* browser,
    std::unique_ptr<CrossDeviceSigninQrBubbleAnchor> anchor_delegate,
    GURL qr_code_url,
    base::OnceClosure closing_callback,
    CrossDeviceSigninPromoEntryPoint entry_point) {
  CHECK(anchor_delegate);
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  const bool should_close_on_deactivate =
      anchor_delegate->ShouldCloseOnDeactivate();
  views::BubbleAnchor anchor = anchor_delegate->GetBubbleAnchor();

  auto web_view = std::make_unique<CrossDeviceSigninQrWebView>(
      browser, std::move(anchor_delegate));
  CrossDeviceSigninQrWebView* raw_web_view = web_view.get();
  base::WeakPtr<CrossDeviceSigninQrWebView> weak_web_view =
      web_view->GetWeakPtr();

  base::OnceCallback<void(views::Widget::ClosedReason)> on_widget_closing =
      base::BindOnce(&CrossDeviceSigninQrWebView::OnWidgetClosingSynchronously,
                     weak_web_view);

  base::OnceClosure cleanup_closure = base::BindOnce(
      [](base::WeakPtr<CrossDeviceSigninQrWebView> web_view,
         base::OnceClosure closing_callback) {
        if (web_view) {
          web_view->ReleaseAnchorState();
        }
        if (closing_callback) {
          std::move(closing_callback).Run();
        }
      },
      weak_web_view, std::move(closing_callback));

  web_view->LoadInitialURL(
      CrossDeviceSigninQrBubbleUI::GetURLWithEntryPoint(entry_point));
  CrossDeviceSigninQrBubbleUI* qr_bubble_ui =
      web_view->GetWebContents()
          ->GetWebUI()
          ->GetController()
          ->GetAs<CrossDeviceSigninQrBubbleUI>();
  if (qr_bubble_ui) {
    qr_bubble_ui->Initialize(std::move(qr_code_url));
  }
  // An initial non-zero height is required on macOS to ensure WebContents
  // allocates a valid initial viewport for rendering before auto-resize.
  constexpr int kPlaceholderHeight = 200;
  web_view->SetPreferredSize(gfx::Size(kWebViewWidth, kPlaceholderHeight));
  web_view->GetWebContents()->SetPageBaseBackgroundColor(SK_ColorTRANSPARENT);

  ui::DialogModel::Builder dialog_model_builder;
  dialog_model_builder.SetInternalName("CrossDeviceSigninQrBubbleViews")
      .SetTitle(l10n_util::GetStringUTF16(
          CrossDeviceSigninQrBubbleUI::GetTitleResourceId(entry_point)))
      .SetDialogDestroyingCallback(std::move(cleanup_closure))
      .OverrideShowCloseButton(true)
      .AddCustomField(
          std::make_unique<views::BubbleDialogModelHost::CustomView>(
              std::move(web_view),
              views::BubbleDialogModelHost::FieldType::kControl),
          kCrossDeviceSigninQrBubbleWebViewElementId);
  if (!should_close_on_deactivate) {
    dialog_model_builder.DisableCloseOnDeactivate();
  }

  auto arrow = views::BubbleBorder::TOP_RIGHT;
  if (anchor.IsNull()) {
    // Fallback to floating the bubble in the center of the browser window if
    // the avatar button or omnibox page action is unavailable (e.g. in a popup
    // window without a toolbar).
    anchor = views::BubbleAnchor(browser_view);
    arrow = views::BubbleBorder::FLOAT;
  }

  auto bubble = std::make_unique<views::BubbleDialogModelHost>(
      dialog_model_builder.Build(), anchor, arrow);
  raw_web_view->ConfigureBubble(*bubble, anchor);
  bubble->set_margins(gfx::Insets());
  bubble->set_fixed_width(kDialogWidth);

  return CrossDeviceSigninQrBubble(std::move(bubble),
                                   std::move(on_widget_closing));
}
