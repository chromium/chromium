// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_WEBUI_OMNIBOX_POPUP_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_WEBUI_OMNIBOX_POPUP_VIEW_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

class LocationBarView;
class RealboxHandler;

// A WebView to display WebUI suggestions for OmniboxPopupViewWebUI.  This class
// is an implementation detail and is not expected to grow or change much with
// omnibox changes.  The concern of this class is presentation only, i.e. Views
// and Widgets.  For omnibox logic concerns and communication between native
// omnibox code and the WebUI code, work with OmniboxPopupViewWebUI directly.
class WebUIOmniboxPopupView : public views::WebView,
                              public views::WidgetObserver {
 public:
  METADATA_HEADER(WebUIOmniboxPopupView);
  explicit WebUIOmniboxPopupView(LocationBarView* location_bar_view);
  WebUIOmniboxPopupView(const WebUIOmniboxPopupView&) = delete;
  WebUIOmniboxPopupView& operator=(const WebUIOmniboxPopupView&) = delete;
  ~WebUIOmniboxPopupView() override;

  // Show or hide the popup widget with web view.
  void Show();
  void Hide();

  // Get the handler for communicating with the WebUI interface.
  RealboxHandler* GetWebUIHandler();

  // views::WidgetObserver:
  void OnWidgetDestroyed(views::Widget* widget) override;

  // Returns the target popup bounds in screen coordinates based on the bounds
  // of |location_bar_view_|.
  gfx::Rect GetTargetBounds() const;

 private:
  // Remove observation and reset widget, optionally requesting it to close.
  void ReleaseWidget(bool close);

  // The location bar view that owns owners of this and thus outlives this.
  base::raw_ptr<LocationBarView> location_bar_view_;

  // Created by this, closed by this; owned and destroyed by OS.
  base::raw_ptr<views::Widget> widget_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_WEBUI_OMNIBOX_POPUP_VIEW_H_
