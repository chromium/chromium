// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_PRESENTER_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_PRESENTER_H_

#include "base/memory/weak_ptr.h"
#include "content/public/browser/render_frame_host.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

class LocationBarView;
class OmniboxController;
class RealboxHandler;

// An assistant class for OmniboxPopupViewWebUI, this manages a WebView and a
// Widget to present WebUI suggestions.  This class is an implementation detail
// and is not expected to grow or change much with omnibox changes.  The concern
// of this class is presentation only, i.e. Views and Widgets.  For omnibox
// logic concerns and communication between native omnibox code and the WebUI
// code, work with OmniboxPopupViewWebUI directly.
class OmniboxPopupPresenter : public views::WebView,
                              public views::WidgetObserver {
 public:
  METADATA_HEADER(OmniboxPopupPresenter);
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
  RealboxHandler* GetHandler();

  // views::WebView
  void FrameSizeChanged(content::RenderFrameHost* render_frame_host,
                        const gfx::Size& frame_size) override;

  // views::WidgetObserver:
  void OnWidgetDestroyed(views::Widget* widget) override;

 private:
  friend class OmniboxPopupViewWebUITest;

  // Block until handler is ready.
  void WaitForHandler();
  void WaitInternal(base::RepeatingClosure* closure);

  // Tells whether the WebUI handler is loaded and ready to receive calls.
  bool IsHandlerReady();

  // Remove observation and reset widget, optionally requesting it to close.
  void ReleaseWidget(bool close);

  // The location bar view that owns owners of this and thus outlives this.
  raw_ptr<LocationBarView> location_bar_view_;

  // Created by this, closed by this; owned and destroyed by OS.
  raw_ptr<views::Widget> widget_;

  // Flags whether this waited for the handler and recorded associated metrics.
  bool waited_for_handler_;

  // Needed in case the browser shuts down and destructs the presenter while
  // waiting for handler. It's an edge case, but theoretically possible.
  base::WeakPtrFactory<OmniboxPopupPresenter> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_PRESENTER_H_
