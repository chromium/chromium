// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_PRESENTER_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_PRESENTER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/searchbox/webui_omnibox_handler.h"
#include "content/public/browser/render_frame_host.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

class LocationBarView;
class OmniboxController;
class OmniboxPopupWebUIContent;

// An assistant class for OmniboxPopupViewWebUI, this manages a WebView and a
// Widget to present WebUI suggestions.  This class is an implementation detail
// and is not expected to grow or change much with omnibox changes.  The concern
// of this class is presentation only, i.e. Views and Widgets.  For omnibox
// logic concerns and communication between native omnibox code and the WebUI
// code, work with OmniboxPopupViewWebUI directly.
class OmniboxPopupPresenter : public views::ViewObserver {
 public:
  OmniboxPopupPresenter(LocationBarView* location_bar_view,
                        OmniboxController* controller);
  OmniboxPopupPresenter(const OmniboxPopupPresenter&) = delete;
  OmniboxPopupPresenter& operator=(const OmniboxPopupPresenter&) = delete;
  ~OmniboxPopupPresenter() override;

  // Show or hide the popup widget with web view.
  void Show();
  void Hide();

  // Tells whether the popup widget exists.
  bool IsShown() const;

  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* observed_view) override;

  void SetWidgetContentHeight(int content_height);

 private:
  friend class OmniboxPopupViewWebUITest;

  void OnWidgetClosed(views::Widget::ClosedReason closed_reason);

  // Remove observation and reset widget, optionally requesting it to close.
  void ReleaseWidget();

  // Returns the webui content, either from the owned pointer or from the
  // content of the widget_.
  OmniboxPopupWebUIContent* GetOmniboxPopupWebUIContent();

  // The location bar view that owns `this`.
  const raw_ptr<LocationBarView> location_bar_view_;

  // The Omnibox WebUI popup contents. It is held here when the widget_ isn't
  // being shown.
  std::unique_ptr<OmniboxPopupWebUIContent> owned_omnibox_popup_webui_content_;

  // The popup widget that contains this WebView. Created and closed by `this`;
  // owned and destroyed by the OS.
  std::unique_ptr<views::Widget> widget_;

  // Whether or not the WebUI popup includes the `location_bar_view` cutout.
  bool include_location_bar_cutout_ = false;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_PRESENTER_H_
