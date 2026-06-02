// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_base_content.h"

#include <string_view>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/omnibox_popup_file_selector.h"
#include "chrome/browser/ui/views/omnibox/omnibox_context_menu.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_aim_presenter.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_tab_selection_listener.h"
#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_web_contents_helper.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/grit/generated_resources.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/permissions/permission_request_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/widget/widget.h"

OmniboxPopupWebUIBaseContent::OmniboxPopupWebUIBaseContent(
    OmniboxPopupPresenterBase* presenter,
    LocationBar* location_bar,
    OmniboxController* controller,
    bool top_rounded_corners)
    : views::WebView(location_bar->GetProfile()),
      popup_presenter_(presenter),
      location_bar_(location_bar),
      controller_(controller),
      top_rounded_corners_(top_rounded_corners),
      last_location_bar_width_(location_bar->BoundsInScreen().width()) {
  location_bar_->AddLocationBarObserver(this);
}

OmniboxPopupWebUIBaseContent::~OmniboxPopupWebUIBaseContent() {
  location_bar_->RemoveLocationBarObserver(this);
}

void OmniboxPopupWebUIBaseContent::AddedToWidget() {
  views::WebView::AddedToWidget();
  holder()->SetCornerRadii(GetRoundedCornerRadii());
}

gfx::RoundedCornersF OmniboxPopupWebUIBaseContent::GetRoundedCornerRadii()
    const {
  const float corner_radius =
      views::LayoutProvider::Get()->GetCornerRadiusMetric(
          views::ShapeContextTokens::kOmniboxExpandedRadius);
  return gfx::RoundedCornersF(top_rounded_corners_ ? corner_radius : 0,
                              top_rounded_corners_ ? corner_radius : 0,
                              corner_radius, corner_radius);
}

void OmniboxPopupWebUIBaseContent::OnLocationBarBoundsChanged() {
  // Track if the location bar width actually changed. This indicates a browser
  // window resize. In that case, bypass the height debouncer to prevent
  // flickering due to being 'behind' during actual resizes with rapid changes.
  const int location_bar_width = location_bar_->BoundsInScreen().width();
  is_window_resizing_ = (location_bar_width != last_location_bar_width_);
  last_location_bar_width_ = location_bar_width;

  int width =
      location_bar_width +
      RoundedOmniboxResultsFrame::GetLocationBarAlignmentInsets().width();

  if (popup_presenter_) {
    width = std::max(width, popup_presenter_->get_minimum_size().width());
  }

  // Update the auto-resize limits for WebUI so that the WebUI updates
  // immediately. If deferred, WebUI content relying on '100%' would render
  // using the outdated width. This causes the WebUI to be smaller or bigger
  // than intended.
  gfx::Size min_size(width, 1);
  gfx::Size max_size(width, INT_MAX);
  if (auto* web_contents = GetWrappedWebContents()) {
    if (auto* render_widget_host_view =
            web_contents->GetRenderWidgetHostView()) {
      render_widget_host_view->EnableAutoResize(min_size, max_size);
    }
  }

  // Defer synchronizing the View popup widget (the actual visible popup)
  // bounds until after the location bar finishes its layout pass. This way,
  // the location bar does not overwrite the width and the minimum width is
  // respected.

  if (has_pending_synchronize_) {
    return;
  }
  has_pending_synchronize_ = true;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::WeakPtr<OmniboxPopupWebUIBaseContent> self) {
                       if (self) {
                         self->has_pending_synchronize_ = false;
                         if (self->popup_presenter_) {
                           self->popup_presenter_->SynchronizePopupBounds();
                         }
                       }
                     },
                     weak_factory_.GetWeakPtr()));
}

void OmniboxPopupWebUIBaseContent::CloseUI() {
  // If the popup state is not shown, don't take any action. Closing the UI
  // multiple times can result in incorrect state transitions from OnClose.
  if (!is_shown_) {
    return;
  }

  is_shown_ = false;

  // Update the popup state manager that the popup is closing.
  // LocationBarView is subscribed to state changes and will close the widget.
  controller()->popup_state_manager()->SetPopupState(OmniboxPopupState::kNone);
}

void OmniboxPopupWebUIBaseContent::ShowUI() {
  // This is a signal from the WebUIContentsWrapper::Host. We use this signal to
  // check if the renderer crashes. If the renderer process has crashed, reset
  // the content URL and create a new renderer.
  if (contents_wrapper_->web_contents() &&
      contents_wrapper_->web_contents()->IsCrashed()) {
    base::UmaHistogramBoolean(
        base::StrCat({GetMetricPrefix(), ".CrashRecovery"}), true);
    LoadContent();
  } else {
    base::UmaHistogramBoolean(
        base::StrCat({GetMetricPrefix(), ".CrashRecovery"}), false);
  }
  SetWebContents(contents_wrapper_->web_contents());

  // The View may have changed, so this reinstates auto-resizing to prevent
  // the omnibox from staying collapsed until a resize is observed.
  OnLocationBarBoundsChanged();

  is_shown_ = true;
}

void OmniboxPopupWebUIBaseContent::ShowCustomContextMenu(
    gfx::Point point,
    std::unique_ptr<ui::MenuModel> menu_model) {
  ConvertPointToScreen(this, &point);
  context_menu_ = std::make_unique<OmniboxContextMenu>(
      GetWidget(), popup_presenter_->delegate().GetOmniboxPopupFileSelector(),
      popup_presenter_->delegate()
          .GetOmniboxPopupAimPresenter()
          ->GetWebUIContent()
          ->GetWrappedWebContents(),
      base::BindRepeating(&OmniboxPopupWebUIBaseContent::OnMenuClosed,
                          base::Unretained(this)));
  context_menu_->RunMenuAt(point, ui::mojom::MenuSourceType::kMouse);
}

void OmniboxPopupWebUIBaseContent::ResizeDueToAutoResize(
    content::WebContents* source,
    const gfx::Size& new_size) {
  WebView::ResizeDueToAutoResize(source, new_size);

  // If the resize was triggered by a browser window resize, bypass the
  // height debouncer to resize immediately and keep height/width updates in
  // sync. This prevents flickering during rapid continuous window resizes.
  const bool is_resizing = is_window_resizing_;
  is_window_resizing_ = false;

  if (popup_presenter_->ShouldDeferUntilVisualStateReady().has_value() ||
      is_resizing) {
    debounce_resize_timer_.Stop();
    popup_presenter_->OnContentHeightChanged(new_size.height());
  } else {
    // Debounce the resize event by 2 frame's time (assuming 60 Hz) to avoid
    // flickering issues when the renderer sends a transient initial size.
    // The issue is manifested as the popup being clipped at the top.
    // This happens when:
    // 1. Widget::Show() is called, then
    // 2. SetBounds() is called with a smaller height.
    // 3. a new frame is not generated timely after resize.
    // As a result, the widget displays an old image that has an taller height,
    // hence clipped.
    //
    // This debouncer suppresses the resize in step #2. The resize comes
    // from the state when the WebUI document briefly contains empty suggestion
    // result.
    //
    // TODO(crbug.com/474369306): there is a race condition between widget show
    // and WebUI document update. The widget is shown too early. Remove the
    // debouncer after making the JS initiate the widget show.
    debounce_resize_timer_.Start(
        FROM_HERE, base::Seconds(2) / 60,
        base::BindOnce(&OmniboxPopupPresenterBase::OnContentHeightChanged,
                       base::Unretained(popup_presenter_), new_size.height()));
  }
}

bool OmniboxPopupWebUIBaseContent::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  if (event.GetType() == input::NativeWebKeyboardEvent::Type::kRawKeyDown &&
      event.windows_key_code == ui::VKEY_ESCAPE) {
    return controller_->edit_model()->OnEscapeKeyPressed();
  }
  return unhandled_keyboard_event_handler_.HandleKeyboardEvent(
      event, GetFocusManager());
}

void OmniboxPopupWebUIBaseContent::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  // Handle the media access requests for voice search by routing them through
  // `MediaCaptureDevicesDispatcher`.
  MediaCaptureDevicesDispatcher::GetInstance()->ProcessMediaAccessRequest(
      web_contents, request, std::move(callback), /*extension=*/nullptr);
}

void OmniboxPopupWebUIBaseContent::SetContentURL(std::string_view url) {
  content_url_ = GURL(url);
  LoadContent();
}

void OmniboxPopupWebUIBaseContent::LoadContent() {
  DCHECK(!content_url_.is_empty());
  contents_wrapper_ = std::make_unique<WebUIContentsWrapperT<OmniboxPopupUI>>(
      content_url_, location_bar_->GetProfile(), IDS_TASK_MANAGER_OMNIBOX);
  contents_wrapper_->SetHost(weak_factory_.GetWeakPtr());
  SetWebContents(contents_wrapper_->web_contents());
  // LocationBarView can be instantiated in windows that do not have a
  // Browser object (i.e Captive Portal). In that case, features depending on
  // the browser are not supported and should be skipped.
  if (Browser* browser = location_bar_->GetBrowser()) {
    webui::SetBrowserWindowInterface(contents_wrapper_->web_contents(),
                                     browser);
    tab_selection_listener_ =
        std::make_unique<OmniboxPopupTabSelectionListener>(
            weak_factory_.GetWeakPtr(), browser->tab_strip_model());
  }
  // Make the OmniboxController available to the OmniboxPopupUI.
  OmniboxPopupWebContentsHelper::CreateForWebContents(GetWebContents());
  OmniboxPopupWebContentsHelper::FromWebContents(GetWebContents())
      ->set_omnibox_controller(controller_);

  // Set ViewType::kComponent so `ChromeSpeechRecognitionManagerDelegate`
  // allows speech recognition in `CheckRenderFrameType()`.
  extensions::SetViewType(contents_wrapper_->web_contents(),
                          extensions::mojom::ViewType::kComponent);
  // Create PermissionRequestManager explicitly for this WebContents.
  // The permission bubble will anchor to the browser window via
  // BrowserWindowInterface.
  permissions::PermissionRequestManager::CreateForWebContents(GetWebContents());

  OnLocationBarBoundsChanged();
  if (base::FeatureList::IsEnabled(omnibox::kOmniboxWebUIPopupMarkAsHidden)) {
    GetWebContents()->WasHidden();
  }
}

void OmniboxPopupWebUIBaseContent::Detach() {
  if (!popup_presenter_->ShouldDetachWebContentsOnHide()) {
    return;
  }
  // This removes the content from being considered for rendering by the
  // compositor while the popup is closed. The content is re-inserted right
  // before the view is displayed. This has the effect of tossing out old,
  // stale content in order to eliminiate it from being briefly displayed
  // while the new content is rendered. This improves visual performance
  // by eliminating that jank and stutter.
  // Under the hood, this forces the contents to clear the SurfaceId to keep
  // the GPU from embedding the content. By not deleting the contents we keep
  // the renderer alive, so when it is re-displayed it is much faster.
  SetWebContents(nullptr);
}

content::WebContents* OmniboxPopupWebUIBaseContent::GetWrappedWebContents() {
  return contents_wrapper_ ? contents_wrapper_->web_contents() : nullptr;
}

void OmniboxPopupWebUIBaseContent::OnMenuClosed() {
  std::move(context_menu_).reset();
  OnContextMenuClosed();
  // Synthesize a mouse leave event from the context menu to trigger
  // re-rendering of the web ui pop up state. This is to ensure entrypoint
  // button to the context menu does not get stuck in the :hover state.
  if (auto* web_contents = GetWebContents()) {
    if (auto* rwh =
            web_contents->GetPrimaryMainFrame()->GetRenderWidgetHost()) {
      blink::WebMouseEvent mouse_event(blink::WebInputEvent::Type::kMouseLeave,
                                       blink::WebInputEvent::kNoModifiers,
                                       base::TimeTicks::Now());
      rwh->ForwardMouseEvent(mouse_event);
    }
  }
}

void OmniboxPopupWebUIBaseContent::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  if (browser_shutdown::HasShutdownStarted()) {
    return;
  }

  base::UmaHistogramEnumeration(
      base::StrCat({GetMetricPrefix(), ".RendererProcessGoneStatus"}), status,
      base::TERMINATION_STATUS_MAX_ENUM);
}

BEGIN_METADATA(OmniboxPopupWebUIBaseContent)
END_METADATA
