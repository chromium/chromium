// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/extensions/api/accessibility_private/accessibility_extension_api.h"

#include "base/logging.h"
#include "chromecast/browser/accessibility/accessibility_manager.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/common/extensions_api/accessibility_private.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/color_parser.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/geometry/transform.h"

namespace {

const char kErrorNotSupported[] = "This API is not supported on this platform.";

}  // namespace

namespace extensions {
namespace cast {
namespace api {

ExtensionFunction::ResponseAction
AccessibilityPrivateSetNativeAccessibilityEnabledFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(args().size() >= 1);
  EXTENSION_FUNCTION_VALIDATE(args()[0].is_bool());
  bool enabled = args()[0].GetBool();
  if (enabled) {
    content::BrowserAccessibilityState::GetInstance()->EnableAccessibility();
  } else {
    content::BrowserAccessibilityState::GetInstance()->DisableAccessibility();
  }
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AccessibilityPrivateSetFocusRingsFunction::Run() {
  std::unique_ptr<accessibility_private::SetFocusRings::Params> params(
      accessibility_private::SetFocusRings::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  auto* accessibility_manager =
      chromecast::shell::CastBrowserProcess::GetInstance()
          ->accessibility_manager();

  for (const accessibility_private::FocusRingInfo& focus_ring_info :
       params->focus_rings) {
    std::vector<gfx::Rect> rects;
    for (const accessibility_private::ScreenRect& rect :
         focus_ring_info.rects) {
      rects.push_back(gfx::Rect(rect.left, rect.top, rect.width, rect.height));
    }

    if (focus_ring_info.color.length() > 0) {
      SkColor color;
      if (!content::ParseHexColorString(focus_ring_info.color, &color))
        return RespondNow(Error("Could not parse hex color"));
      accessibility_manager->SetFocusRingColor(color);
    } else {
      accessibility_manager->ResetFocusRingColor();
    }

    // Move the visible focus ring to cover all of these rects.
    accessibility_manager->SetFocusRing(
        rects, chromecast::FocusRingBehavior::PERSIST_FOCUS_RING);

    // Also update the touch exploration controller so that synthesized
    // touch events are anchored within the focused object.
    if (!rects.empty()) {
      accessibility_manager->SetTouchAccessibilityAnchorPoint(
          rects[0].CenterPoint());
    }
  }

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AccessibilityPrivateSetHighlightsFunction::Run() {
  auto* accessibility_manager =
      chromecast::shell::CastBrowserProcess::GetInstance()
          ->accessibility_manager();

  std::unique_ptr<accessibility_private::SetHighlights::Params> params(
      accessibility_private::SetHighlights::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  std::vector<gfx::Rect> rects;
  for (const accessibility_private::ScreenRect& rect : params->rects) {
    rects.push_back(gfx::Rect(rect.left, rect.top, rect.width, rect.height));
  }

  SkColor color;
  if (!content::ParseHexColorString(params->color, &color))
    return RespondNow(Error("Could not parse hex color"));

  // Set the highlights to cover all of these rects.
  accessibility_manager->SetHighlights(rects, color);

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AccessibilityPrivateSetKeyboardListenerFunction::Run() {
  LOG(ERROR) << "AccessibilityPrivateSetKeyboardListenerFunction";
  return RespondNow(Error(kErrorNotSupported));
}

ExtensionFunction::ResponseAction
AccessibilityPrivateDarkenScreenFunction::Run() {
  LOG(ERROR) << "AccessibilityPrivateDarkenScreenFunction";
  return RespondNow(Error(kErrorNotSupported));
}

ExtensionFunction::ResponseAction
AccessibilityPrivateSetNativeChromeVoxArcSupportForCurrentAppFunction::Run() {
  LOG(ERROR) << "AccessibilityPrivateSetNativeChromeVoxArcSupportForCurrentAppF"
                "unction";
  return RespondNow(Error(kErrorNotSupported));
}

ExtensionFunction::ResponseAction
AccessibilityPrivateSendSyntheticKeyEventFunction::Run() {
  LOG(ERROR) << "AccessibilityPrivateSendSyntheticKeyEventFunction";
  return RespondNow(Error(kErrorNotSupported));
}

ExtensionFunction::ResponseAction
AccessibilityPrivateSendSyntheticMouseEventFunction::Run() {
  // Translate the mouse event to touch event so that touch exploration
  // controller can handle them.
  std::unique_ptr<accessibility_private::SendSyntheticMouseEvent::Params>
      params = accessibility_private::SendSyntheticMouseEvent::Params::Create(
          args());
  EXTENSION_FUNCTION_VALIDATE(params);
  accessibility_private::SyntheticMouseEvent* mouse_data = &params->mouse_event;

  ui::EventType type = ui::ET_UNKNOWN;
  switch (mouse_data->type) {
    case accessibility_private::SYNTHETIC_MOUSE_EVENT_TYPE_PRESS:
      type = ui::ET_TOUCH_PRESSED;
      break;
    case accessibility_private::SYNTHETIC_MOUSE_EVENT_TYPE_RELEASE:
      type = ui::ET_TOUCH_RELEASED;
      break;
    default:
      // skip other gestures.
      return RespondNow(NoArguments());
  }

  // Locations are assumed to be in screen coordinates.
  gfx::Point location_in_screen(mouse_data->x, mouse_data->y);
  auto* host = chromecast::shell::CastBrowserProcess::GetInstance()
                   ->accessibility_manager()
                   ->window_tree_host();
  DCHECK(host);

  ui::PointerDetails pointer_details;
  pointer_details.pointer_type = ui::EventPointerType::kTouch;
  gfx::Point location(mouse_data->x, mouse_data->y);

  std::unique_ptr<ui::TouchEvent> touch_event =
      std::make_unique<ui::TouchEvent>(type, gfx::Point(),
                                       ui::EventTimeForNow(), pointer_details);
  touch_event->set_location(location);
  touch_event->set_root_location(location);
  touch_event->UpdateForRootTransform(
      host->GetRootTransform(),
      host->GetRootTransformForLocalEventCoordinates());
  // Still go through the event rewriters.
  host->SendEventToSink(touch_event.get());
  return RespondNow(NoArguments());
}

}  // namespace api
}  // namespace cast
}  // namespace extensions
