// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/extensions/api/accessibility_private/accessibility_extension_api.h"

#include "chromecast/browser/accessibility/accessibility_manager.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/common/extensions_api/accessibility_private.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/service_manager_connection.h"
#include "extensions/common/image_util.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace {

const char kErrorNotSupported[] = "This API is not supported on this platform.";

}  // namespace

namespace extensions {
namespace cast {
namespace api {

ExtensionFunction::ResponseAction
AccessibilityPrivateSetNativeAccessibilityEnabledFunction::Run() {
  bool enabled = false;
  EXTENSION_FUNCTION_VALIDATE(args_->GetBoolean(0, &enabled));
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
      accessibility_private::SetFocusRings::Params::Create(*args_));
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
      if (!extensions::image_util::ParseHexColorString(focus_ring_info.color,
                                                       &color))
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
      accessibility_private::SetHighlights::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  std::vector<gfx::Rect> rects;
  for (const accessibility_private::ScreenRect& rect : params->rects) {
    rects.push_back(gfx::Rect(rect.left, rect.top, rect.width, rect.height));
  }

  SkColor color;
  if (!extensions::image_util::ParseHexColorString(params->color, &color))
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

}  // namespace api
}  // namespace cast
}  // namespace extensions
