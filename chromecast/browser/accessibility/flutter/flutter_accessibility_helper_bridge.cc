// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/accessibility/flutter/flutter_accessibility_helper_bridge.h"

#include <utility>

#include "base/logging.h"
#include "chromecast/browser/accessibility/accessibility_manager.h"
#include "chromecast/browser/accessibility/proto/gallium_server_accessibility.grpc.pb.h"
#include "chromecast/browser/cast_browser_process.h"
#include "components/exo/fullscreen_shell_surface.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/surface.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "ui/accessibility/aura/aura_window_properties.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace chromecast {
namespace gallium {
namespace accessibility {

using ::gallium::castos::
    OnAccessibilityActionRequest_AccessibilityActionType_CUSTOM_ACTION;
using ::gallium::castos::
    // NOLINTNEXTLINE(whitespace/line_length)
    OnAccessibilityActionRequest_AccessibilityActionType_DID_GAIN_ACCESSIBILITY_FOCUS;
using ::gallium::castos::
    // NOLINTNEXTLINE(whitespace/line_length)
    OnAccessibilityActionRequest_AccessibilityActionType_DID_LOSE_ACCESSIBILITY_FOCUS;
using ::gallium::castos::
    OnAccessibilityActionRequest_AccessibilityActionType_SCROLL_DOWN;
using ::gallium::castos::
    OnAccessibilityActionRequest_AccessibilityActionType_SCROLL_LEFT;
using ::gallium::castos::
    OnAccessibilityActionRequest_AccessibilityActionType_SCROLL_RIGHT;
using ::gallium::castos::
    OnAccessibilityActionRequest_AccessibilityActionType_SCROLL_UP;
using ::gallium::castos::
    OnAccessibilityActionRequest_AccessibilityActionType_SET_SELECTION;
using ::gallium::castos::
    OnAccessibilityActionRequest_AccessibilityActionType_SHOW_ON_SCREEN;
using ::gallium::castos::
    OnAccessibilityActionRequest_AccessibilityActionType_TAP;

FlutterAccessibilityHelperBridge::FlutterAccessibilityHelperBridge(
    Delegate* bridge_delegate,
    content::BrowserContext* browser_context)
    : tree_source_(
          std::make_unique<AXTreeSourceFlutter>(this, browser_context)),
      bridge_delegate_(bridge_delegate) {}

FlutterAccessibilityHelperBridge::~FlutterAccessibilityHelperBridge() = default;

void FlutterAccessibilityHelperBridge::AccessibilityStateChanged(bool value) {
  tree_source_->SetAccessibilityEnabled(value);
  if (value) {
    aura::Window* window = chromecast::shell::CastBrowserProcess::GetInstance()
                               ->accessibility_manager()
                               ->window_tree_host()
                               ->window();

    // Find the full screen shell surface for the exo::Surface representing
    // the ui.  We must ensure our tree id is the child ax tree id for
    // that view so when the root window is serialized, the flutter ax tree
    // will be parented by that view.
    bool found = false;
    if (window) {
      for (aura::Window* child : window->children()) {
        exo::Surface* surface = exo::GetShellRootSurface(child);
        if (surface) {
          views::Widget* widget =
              views::Widget::GetWidgetForNativeWindow(child);
          if (widget) {
            exo::FullscreenShellSurface* full_screen_shell_surface =
                static_cast<exo::FullscreenShellSurface*>(
                    widget->widget_delegate());
            full_screen_shell_surface->SetChildAxTreeId(
                tree_source_->ax_tree_id());
            full_screen_shell_surface->GetContentsView()
                ->NotifyAccessibilityEvent(ax::mojom::Event::kChildrenChanged,
                                           false);
            child->Focus();
            found = true;
          }
          break;
        }
      }
    }

    if (!found) {
      LOG(ERROR) << "Could not find full screen shell surface for ax tree.";
    }
  }
}

void FlutterAccessibilityHelperBridge::OnAccessibilityEventRequestInternal(
    std::unique_ptr<::gallium::castos::OnAccessibilityEventRequest>
        event_data) {
  // Tell the tree source to serialize these changes.
  tree_source_->NotifyAccessibilityEvent(event_data.get());
}

bool FlutterAccessibilityHelperBridge::OnAccessibilityEventRequest(
    const ::gallium::castos::OnAccessibilityEventRequest* event_data) {
  std::unique_ptr<::gallium::castos::OnAccessibilityEventRequest> event =
      std::make_unique<::gallium::castos::OnAccessibilityEventRequest>(
          *event_data);

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&FlutterAccessibilityHelperBridge::
                                    OnAccessibilityEventRequestInternal,
                                base::Unretained(this), std::move(event)));
  return true;
}

void FlutterAccessibilityHelperBridge::OnAction(const ui::AXActionData& data) {
  // Called by tree source to dispatch ax action to flutter. Translate this
  // to gallium accessibility proto and forward to the delegate for
  // dispatching.
  ::gallium::castos::OnAccessibilityActionRequest request;
  request.set_node_id(data.target_node_id);

  switch (data.action) {
    case ax::mojom::Action::kDoDefault:
      request.set_action_type(
          OnAccessibilityActionRequest_AccessibilityActionType_TAP);
      break;
    case ax::mojom::Action::kScrollToMakeVisible:
      request.set_action_type(
          OnAccessibilityActionRequest_AccessibilityActionType_SHOW_ON_SCREEN);
      break;
    case ax::mojom::Action::kScrollBackward:
      request.set_action_type(
          OnAccessibilityActionRequest_AccessibilityActionType_SCROLL_LEFT);
      tree_source_->NotifyActionResult(data, false);
      break;
    case ax::mojom::Action::kScrollForward:
      request.set_action_type(
          OnAccessibilityActionRequest_AccessibilityActionType_SCROLL_RIGHT);
      tree_source_->NotifyActionResult(data, false);
      break;
    case ax::mojom::Action::kScrollUp:
      request.set_action_type(
          OnAccessibilityActionRequest_AccessibilityActionType_SCROLL_UP);
      tree_source_->NotifyActionResult(data, false);
      break;
    case ax::mojom::Action::kScrollDown:
      request.set_action_type(
          OnAccessibilityActionRequest_AccessibilityActionType_SCROLL_DOWN);
      tree_source_->NotifyActionResult(data, false);
      break;
    case ax::mojom::Action::kScrollLeft:
      request.set_action_type(
          OnAccessibilityActionRequest_AccessibilityActionType_SCROLL_LEFT);
      tree_source_->NotifyActionResult(data, false);
      break;
    case ax::mojom::Action::kScrollRight:
      request.set_action_type(
          OnAccessibilityActionRequest_AccessibilityActionType_SCROLL_RIGHT);
      tree_source_->NotifyActionResult(data, false);
      break;
    case ax::mojom::Action::kCustomAction:
      request.set_action_type(
          OnAccessibilityActionRequest_AccessibilityActionType_CUSTOM_ACTION);
      request.set_custom_action_id(data.custom_action_id);
      break;
    case ax::mojom::Action::kSetAccessibilityFocus:
      request.set_action_type(
          // NOLINTNEXTLINE(whitespace/line_length)
          OnAccessibilityActionRequest_AccessibilityActionType_DID_GAIN_ACCESSIBILITY_FOCUS);
      break;
    case ax::mojom::Action::kClearAccessibilityFocus:
      request.set_action_type(
          // NOLINTNEXTLINE(whitespace/line_length)
          OnAccessibilityActionRequest_AccessibilityActionType_DID_LOSE_ACCESSIBILITY_FOCUS);
      break;
    case ax::mojom::Action::kGetTextLocation:
      request.set_action_type(
          OnAccessibilityActionRequest_AccessibilityActionType_SET_SELECTION);
      request.set_start_index(data.start_index);
      request.set_end_index(data.end_index);
      break;
    default:
      LOG(WARNING) << "Cast ax action " << data.action
                   << " not mapped to flutter action - dropped.";

      return;
  }

  bridge_delegate_->SendAccessibilityAction(request);
}

void FlutterAccessibilityHelperBridge::OnVirtualKeyboardBoundsChange(
    const gfx::Rect& bounds) {
  chromecast::shell::CastBrowserProcess::GetInstance()
      ->accessibility_manager()
      ->SetVirtualKeyboardBounds(bounds);
}

}  // namespace accessibility
}  // namespace gallium
}  // namespace chromecast
