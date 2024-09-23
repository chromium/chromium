// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_PRESENTER_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_PRESENTER_H_

#include "chrome/browser/ui/webui/searchbox/realbox_handler.h"
#include "content/public/browser/render_frame_host.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

class LocationBarView;
class OmniboxController;

// An assistant class for OmniboxPopupViewWebUI, this manages a WebView and a
// Widget to present WebUI suggestions.  This class is an implementation detail
// and is not expected to grow or change much with omnibox changes.  The concern
// of this class is presentation only, i.e. Views and Widgets.  For omnibox
// logic concerns and communication between native omnibox code and the WebUI
// code, work with OmniboxPopupViewWebUI directly.
class OmniboxPopupPresenter : public views::WebView,
                              public views::WidgetObserver,
                              public OmniboxWebUIPopupChangeObserver,
                              public views::ViewObserver {
  METADATA_HEADER(OmniboxPopupPresenter, views::WebView)

 public:
  explicit OmniboxPopupPresenter(LocationBarView* location_bar_view,
                                 OmniboxController* controller);
  OmniboxPopupPresenter(const OmniboxPopupPresenter&) = delete;
  OmniboxPopupPresenter& operator=(const OmniboxPopupPresenter&) = delete;
  ~OmniboxPopupPresenter() override;

  // Show or hide the popup widget with web view.
  void Show();
  void Hide();

  // Tells whether the popup widget exists.
  bool IsShown() const;

  // Get the handler for communicating with the WebUI interface.
  // Returns nullptr if handler is not ready.
  RealboxHandler* GetHandler();

  // views::WidgetObserver:
  void OnWidgetDestroyed(views::Widget* widget) override;

  // RealboxWebUIChangeClient:
  void OnPopupElementSizeChanged(gfx::Size size) override;

  // views::ViewObserver:
  void OnViewBoundsChanged(View* observed_view) override;

 private:
  friend class OmniboxPopupViewWebUITest;

  // Tells whether the WebUI handler is loaded and ready to receive calls.
  bool IsHandlerReady();

  // Remove observation and reset widget, optionally requesting it to close.
  void ReleaseWidget(bool close);

  // The location bar view that owns owners of this and thus outlives this.
  raw_ptr<LocationBarView> location_bar_view_;

  // Created by this, closed by this; owned and destroyed by OS.
  raw_ptr<views::Widget> widget_;

  // Whether any call to `GetHandler` has been made.
  bool requested_handler_;

  // Last reported WebUI element size.
  gfx::Size webui_element_size_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_PRESENTER_H_
