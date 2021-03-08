// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_ACCESSIBILITY_FLUTTER_FLUTTER_ACCESSIBILITY_HELPER_BRIDGE_H_
#define CHROMECAST_BROWSER_ACCESSIBILITY_FLUTTER_FLUTTER_ACCESSIBILITY_HELPER_BRIDGE_H_

#include <memory>

#include "chromecast/browser/accessibility/flutter/ax_tree_source_flutter.h"

using chromecast::accessibility::AXTreeSourceFlutter;

namespace content {
class BrowserContext;
}  // namespace content

namespace gallium {
namespace castos {
class OnAccessibilityEventRequest;
class OnAccessibilityActionRequest;
}  // namespace castos
}  // namespace gallium

namespace chromecast {
namespace gallium {
namespace accessibility {

// FlutterAccessibilityHelperBridge receives Flutter accessibility
// events from gallium, translates them to chrome tree updates and dispatches
// them to chromecast accessibility services.
class FlutterAccessibilityHelperBridge : public AXTreeSourceFlutter::Delegate {
 public:
  class Delegate {
   public:
    virtual void SendAccessibilityAction(
        ::gallium::castos::OnAccessibilityActionRequest request) = 0;

   protected:
    virtual ~Delegate() = default;
  };

  FlutterAccessibilityHelperBridge(Delegate* bridge_delegate,
                                   content::BrowserContext* browser_context);
  FlutterAccessibilityHelperBridge(const FlutterAccessibilityHelperBridge&) =
      delete;
  ~FlutterAccessibilityHelperBridge() override;
  FlutterAccessibilityHelperBridge& operator=(
      const FlutterAccessibilityHelperBridge&) = delete;

  // Receive an accessibility event from flutter.
  bool OnAccessibilityEventRequest(
      const ::gallium::castos::OnAccessibilityEventRequest* event_data);

  // AXTreeSourceArc::Delegate implementation:
  // Dispatch a chrome accessibility action to flutter.
  void OnAction(const ui::AXActionData& data) override;
  void OnVirtualKeyboardBoundsChange(const gfx::Rect& bounds) override;

  void AccessibilityStateChanged(bool value);

 private:
  void OnAccessibilityEventRequestInternal(
      std::unique_ptr<::gallium::castos::OnAccessibilityEventRequest>
          event_data);

  std::unique_ptr<AXTreeSourceFlutter> tree_source_;
  Delegate* bridge_delegate_;
};

}  // namespace accessibility
}  // namespace gallium
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_ACCESSIBILITY_FLUTTER_FLUTTER_ACCESSIBILITY_HELPER_BRIDGE_H_
