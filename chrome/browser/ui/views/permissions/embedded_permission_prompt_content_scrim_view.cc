// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_content_scrim_view.h"

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_base_content.h"
#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "components/permissions/permissions_client.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/views/background.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace {
constexpr char kWidgetName[] = "EmbeddedPermissionPromptContentScrimWidget";
}

// Only require web contents instance to be passed in and not widget instance
// (for window sizing) since web contents knows which widget it belongs to.
EmbeddedPermissionPromptContentScrimView::
    EmbeddedPermissionPromptContentScrimView(base::WeakPtr<Delegate> delegate,
                                             content::WebContents& web_contents,
                                             bool should_dismiss_on_click)
    : content::WebContentsObserver(&web_contents),
      delegate_(std::move(delegate)),
      should_dismiss_on_click_(should_dismiss_on_click) {
  SetProperty(views::kElementIdentifierKey, kContentScrimViewId);
  // Observe the top-level widget to ensure the scrim view is resized when the
  // OS-level window bounds change.
  views::Widget* widget = views::Widget::GetTopLevelWidgetForNativeView(
      web_contents.GetContentNativeView());
  if (widget) {
    widget_observation_.Observe(widget);

    // Attempt to downcast the client contents view (what view the scrim is
    // over). `rounded_frame` will be `nullptr` if
    // `views::AsViewClass` wrapper (which handles error-prone downcasts
    // safely) does not return a `RoundedOmniboxResultsFrame` instance. An
    // example where an instance does not exist is when the prompt is shown on
    // a normal browser page instead of the WebUI Omnibox popup.
    auto* rounded_frame = views::AsViewClass<RoundedOmniboxResultsFrame>(
        widget->GetClientContentsView());
    if (rounded_frame) {
      // If it is a `RoundedOmniboxResultsFrame`, we check if it hosts WebUI
      // omnibox popup content.
      auto* omnibox_content = rounded_frame->GetOmniboxPopupWebUIBaseContent();
      if (omnibox_content) {
        // Enable paint-to-layer on this view and clip it to match the popup's
        // rounded corners, keeping transparent areas clean.
        SetPaintToLayer();
        layer()->SetFillsBoundsOpaquely(false);
        layer()->SetRoundedCornerRadius(
            omnibox_content->GetRoundedCornerRadii());
        layer()->SetIsFastRoundedCorner(true);
      }
    }
  }
}

EmbeddedPermissionPromptContentScrimView::
    ~EmbeddedPermissionPromptContentScrimView() = default;

// static
std::unique_ptr<views::Widget>
EmbeddedPermissionPromptContentScrimView::CreateScrimWidget(
    base::WeakPtr<Delegate> delegate,
    SkColor color,
    bool should_dismiss_on_click) {
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  auto permission_prompt_delegate = delegate->GetPermissionPromptDelegate();
  CHECK(permission_prompt_delegate);
  auto* web_contents = permission_prompt_delegate->GetAssociatedWebContents();
  auto* top_level_widget = views::Widget::GetTopLevelWidgetForNativeView(
      web_contents->GetContentNativeView());
  if (!top_level_widget) {
    return nullptr;
  }
  params.parent = top_level_widget->GetNativeView();
  params.bounds = web_contents->GetContainerBounds();
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.accept_events = true;
  // The scrim should not be activatable to prevent it from stealing
  // OS-level focus when clicked, which ensures focus remains on the
  // browser window during prompt dismissal.
  params.activatable = views::Widget::InitParams::Activatable::kNo;
  params.name = kWidgetName;
  auto widget = std::make_unique<views::Widget>();
  widget->Init(std::move(params));

  auto content_scrim_view =
      std::make_unique<EmbeddedPermissionPromptContentScrimView>(
          delegate, *web_contents, should_dismiss_on_click);
  content_scrim_view->SetBackground(views::CreateSolidBackground(color));
  widget->SetContentsView(std::move(content_scrim_view));
  widget->SetVisibilityChangedAnimationsEnabled(false);
  widget->Show();
  return widget;
}

bool EmbeddedPermissionPromptContentScrimView::OnMousePressed(
    const ui::MouseEvent& event) {
  if (delegate_ && should_dismiss_on_click_) {
    delegate_->DismissScrim();
  }
  return true;
}

void EmbeddedPermissionPromptContentScrimView::OnGestureEvent(
    ui::GestureEvent* event) {
  if (delegate_ && should_dismiss_on_click_ &&
      (event->type() == ui::EventType::kGestureTap ||
       event->type() == ui::EventType::kGestureDoubleTap)) {
    delegate_->DismissScrim();
  }
}

// content::WebContentsObserver:
// Handles internal layout changes where the OS-level window does not change
// size, but the web page area does (example: the user drags the Side Panel or
// DevTools).
void EmbeddedPermissionPromptContentScrimView::FrameSizeChanged(
    content::RenderFrameHost* render_frame_host,
    const gfx::Size& frame_size) {
  if (!delegate_ || !GetWidget() || !web_contents()) {
    return;
  }

  // Ignore all iframes in the page but the main page.
  if (render_frame_host != web_contents()->GetPrimaryMainFrame()) {
    return;
  }
  GetWidget()->SetBounds(web_contents()->GetContainerBounds());
}

// views::WidgetObserver:
// Safely unregisters the observer when the top-level window is closing.
// This prevents use-after-free crashes if the window dies before this View
// does.
void EmbeddedPermissionPromptContentScrimView::OnWidgetDestroyed(
    views::Widget* widget) {
  widget_observation_.Reset();
}

// views::WidgetObserver:
// Handles OS-level window changes (example: the user maximizes the Chrome
// window or drags the outer corner to resize it).
void EmbeddedPermissionPromptContentScrimView::OnWidgetBoundsChanged(
    views::Widget* widget,
    const gfx::Rect& new_bounds) {
  if (!delegate_ || !GetWidget() || !web_contents()) {
    return;
  }

  // Avoid using the `permission_prompt_delegate` since web_contents
  // is passed in as instance to this observer class. This allows for safety
  // check that `web_contents()` is not null while the widget is still alive.
  // Ignore `new_bounds` since it represents the full window, but only the web
  // page matters.
  GetWidget()->SetBounds(web_contents()->GetContainerBounds());
}

BEGIN_METADATA(EmbeddedPermissionPromptContentScrimView)
END_METADATA

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(EmbeddedPermissionPromptContentScrimView,
                                      kContentScrimViewId);

EmbeddedPermissionPromptContentScrim::EmbeddedPermissionPromptContentScrim(
    content::WebContents& web_contents,
    permissions::EmbeddedPermissionPromptFlowModel* flow_model)
    : web_contents_(&web_contents), flow_model_(flow_model) {
  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents_);
  BrowserWindowInterface* current_browser =
      tab ? tab->GetBrowserWindowInterface() : nullptr;
  if (!current_browser) {
    current_browser = webui::GetBrowserWindowInterface(web_contents_);
  }
  if (current_browser) {
    if (auto* browser_view =
            BrowserView::GetBrowserViewForBrowser(current_browser)) {
      if (auto* focus_manager = browser_view->GetFocusManager()) {
        previously_focused_view_tracker_.SetView(
            focus_manager->GetFocusedView());
      }
    }
  }

  scoped_ignore_input_events_ = web_contents_->IgnoreInputEvents(std::nullopt);
  if (!permissions::PermissionsClient::Get()
           ->IsPrivilegedInternalWebUIForUIRouting(web_contents_)) {
    if (tab) {
      scoped_tab_modal_ui_ = tab->ShowModalUI();
    }
  }

  content_scrim_widget_ =
      EmbeddedPermissionPromptContentScrimView::CreateScrimWidget(
          weak_factory_.GetWeakPtr(),
          SkColorSetA(web_contents_->GetColorProvider().GetColor(
                          ui::kColorRefNeutral20),
                      0.8 * SK_AlphaOPAQUE),
          /*should_dismiss_on_click=*/true);
}

EmbeddedPermissionPromptContentScrim::~EmbeddedPermissionPromptContentScrim() {
  if (web_contents_->IsBeingDestroyed()) {
    return;
  }
  FocusThenClose();
}

void EmbeddedPermissionPromptContentScrim::DismissScrim() {
  if (flow_model_) {
    flow_model_->DismissScrim();
  }
}

base::WeakPtr<permissions::PermissionPrompt::Delegate>
EmbeddedPermissionPromptContentScrim::GetPermissionPromptDelegate() const {
  if (!flow_model_ || !flow_model_->GetPermissionPromptDelegate()) {
    return nullptr;
  }
  return flow_model_->GetPermissionPromptDelegate()->GetWeakPtr();
}

void EmbeddedPermissionPromptContentScrim::FocusThenClose() {
  // The native Browser UI (the Omnibox) does not have web contents like web
  // pages do. If OS restores focus to the WebContents when the prompt closes,
  // it steals focus from the Omnibox, which triggers `OnKillFocus`
  // and incorrectly collapses the omnibox popup and therefore voice search.
  views::View* previously_focused_view =
      previously_focused_view_tracker_.view();
  // Reset to avoid reuse and any re-entrancy.
  previously_focused_view_tracker_.SetView(nullptr);

  // Only restore focus if the view still exists, is still drawn on screen,
  // and is still capable of receiving focus.
  if (previously_focused_view && previously_focused_view->IsDrawn() &&
      previously_focused_view->IsFocusable()) {
    previously_focused_view->RequestFocus();
  } else {
    // Focus must be restored to the browser before the prompt widget is
    // destroyed to ensure the OS properly targets the browser window when the
    // prompt closes instead of an available window (which, with status race
    // conditions caused by the prompt, does not include Chrome sometimes). This
    // is a particular bug related to how native Windows handles focus after an
    // ambiguous focus release.
    web_contents_->Focus();
  }

  if (content_scrim_widget_) {
    content_scrim_widget_->Close();
    content_scrim_widget_ = nullptr;
    scoped_ignore_input_events_.reset();
  }

  scoped_tab_modal_ui_.reset();
}
