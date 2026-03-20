// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/read_anything/read_anything_immersive_web_view.h"

#include <utility>

#include "base/check.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/read_anything/read_anything_controller.h"
#include "chrome/browser/ui/read_anything/read_anything_enums.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/keycodes/keyboard_codes.h"

ReadAnythingImmersiveWebView::ReadAnythingImmersiveWebView(
    base::OnceClosure on_show_ui_callback,
    std::unique_ptr<WebUIContentsWrapperT<ReadAnythingUntrustedUI>>
        contents_wrapper,
    ReadAnythingOpenTrigger trigger)
    : on_show_ui_callback_(std::move(on_show_ui_callback)),
      contents_wrapper_(std::move(contents_wrapper)),
      trigger_(trigger) {
  SetWebContents(contents_wrapper_->web_contents());
  contents_wrapper_->SetHost(weak_factory_.GetWeakPtr());
  // Calling ReadAnythingImmersiveWebView::ShowUI is not necessary if it's not
  // been shown yet- the WebUI will call ShowUI when it is ready. If the UI has
  // been shown once, the reused WebUI will be available but won't send a new
  // "showUI" message. Manually call ShowUI() to make the view visible.
  auto* controller =
      ReadAnythingControllerGlue::FromWebContents(web_contents())->controller();
  if (controller && controller->has_shown_ui()) {
    ShowUI();
  }
}

ReadAnythingImmersiveWebView::~ReadAnythingImmersiveWebView() = default;

bool ReadAnythingImmersiveWebView::HandleContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  return false;
}

bool ReadAnythingImmersiveWebView::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  if (event.windows_key_code == ui::VKEY_ESCAPE) {
    auto* controller =
        ReadAnythingControllerGlue::FromWebContents(web_contents())
            ->controller();
    if (controller && controller->tab() &&
        controller->tab()->GetBrowserWindowInterface()) {
      controller->tab()
          ->GetBrowserWindowInterface()
          ->GetExclusiveAccessManager()
          ->HandleUserKeyEvent(event);
      return true;
    }
  }
  // Call the unhandled keyboard event handler to allow for default handling
  // and propagation.
  return unhandled_keyboard_event_handler_.HandleKeyboardEvent(
      event, GetFocusManager());
}

std::unique_ptr<WebUIContentsWrapperT<ReadAnythingUntrustedUI>>
ReadAnythingImmersiveWebView::CloseAndTakeContentsWrapper() {
  SetWebContents(nullptr);  // This is necessary to reset the web contents.
  // SetHost cannot be called on a crashed WebContents and will throw a DCHECK
  // crash if it is.
  if (!contents_wrapper_->web_contents()->IsCrashed()) {
    contents_wrapper_->SetHost(nullptr);
  }
  SetVisible(false);

  // Call OnEntryHidden on the Controller
  auto* read_anything_controller = ReadAnythingControllerGlue::FromWebContents(
                                       contents_wrapper_->web_contents())
                                       ->controller();
  CHECK(read_anything_controller);
  read_anything_controller->OnEntryHidden();

  return std::move(contents_wrapper_);
}

// WebUIContentsWrapper::Host:
// Called by the WebUI on its embedder (this class) when the WebUI is ready to
// be shown.
void ReadAnythingImmersiveWebView::ShowUI() {
  if (on_show_ui_callback_) {
    std::move(on_show_ui_callback_).Run();
  }
  SetVisible(true);
  auto* read_anything_controller = ReadAnythingControllerGlue::FromWebContents(
                                       contents_wrapper_->web_contents())
                                       ->controller();
  CHECK(read_anything_controller);
  read_anything_controller->OnEntryShown(trigger_);
}

// Called by the WebUI on its embedder (this class) when the WebUI is ready to
// be closed.
void ReadAnythingImmersiveWebView::CloseUI() {
  // This currently does not do anything and is never called because the
  // ReadAnythingController is currently the one that owns the WebUI by the time
  // it is closed.
}

BEGIN_METADATA(ReadAnythingImmersiveWebView)
END_METADATA
