// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_TOOLBAR_WEB_VIEW_UTIL_MAC_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_TOOLBAR_WEB_VIEW_UTIL_MAC_H_

#include <list>
#include <memory>

#include "base/memory/raw_ref.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"

namespace views {
class WebView;
}  // namespace views

class WebUIToolbarControlDelegate;

//  The approach used by RoundedOmniboxResultsFrame on Mac to forward mouse
//  events received by the popup to the underlying windows ultimately ends up
//  with the event received at Views level at NativeViewHost, which doesn't know
//  what to do with them. This is set up as a fallback handler to receive these,
//  and forward them on further till the WebView. It also reconstructs
//  double- (and triple-) clicks, since they might get broken by the coordinate
//  change of the popup showing. Events are dispatched to the WebView's
//  NSView (RenderWidgetHostViewCocoa), so that they take the same path as
//  events AppKit delivers to it directly.
class WebUIToolbarEventForwarder : public ui::EventHandler {
 public:
  WebUIToolbarEventForwarder(WebUIToolbarControlDelegate& control_delegate,
                             views::WebView& web_view);
  ~WebUIToolbarEventForwarder() override;

  void OnMouseEvent(ui::MouseEvent* event) override;

  void AddedToWidget();
  void RemovedFromWidget();

 private:
  class PressMonitor;

  bool HaveOpenOmniboxPopup();
  ui::MouseEvent* OverallLastAdjusted(const ui::MouseEvent& to_disregard);

  const raw_ref<WebUIToolbarControlDelegate> control_delegate_;
  const raw_ref<views::WebView> web_view_;
  std::unique_ptr<PressMonitor> monitor_;

  // Last mouse press we got, with location adjusted to screen coordinates.
  std::unique_ptr<ui::MouseEvent> last_adjusted_event_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_TOOLBAR_WEB_VIEW_UTIL_MAC_H_
