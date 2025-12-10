// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_PRESENTER_BASE_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_PRESENTER_BASE_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/searchbox/webui_omnibox_handler.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget.h"

class LocationBarView;
class OmniboxPopupWebUIBaseContent;
class RoundedOmniboxResultsFrame;

namespace omnibox {
extern const void* kOmniboxWebUIPopupWidgetId;
}  // namespace omnibox

// A base assistant class for OmniboxPopupViewWebUI, this manages "n" WebViews
// and a Widget to present the WebUI. This class is an implementation detail and
// is not expected to grow or change much with omnibox changes.  The concern of
// this class is presentation only, i.e. Views and Widgets.  For omnibox logic
// concerns and communication between native omnibox code and the WebUI code,
// work with OmniboxPopupViewWebUI directly.
class OmniboxPopupPresenterBase {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kRoundedResultsFrame);
  explicit OmniboxPopupPresenterBase(LocationBarView* location_bar_view);
  OmniboxPopupPresenterBase(const OmniboxPopupPresenterBase&) = delete;
  OmniboxPopupPresenterBase& operator=(const OmniboxPopupPresenterBase&) =
      delete;
  virtual ~OmniboxPopupPresenterBase();

  // Show or hide the popup widget with web view.
  virtual void Show();
  virtual void Hide();

  // Tells whether the popup widget exists.
  bool IsShown() const;

  // Caches the height of the WebUI content, which is then used to compute the
  // popup widget bounds.
  void OnContentHeightChanged(int content_height);

  // Returns the currently "active" Popup content, whichever one is visible or
  // going to be visible within the popup.
  OmniboxPopupWebUIBaseContent* GetWebUIContent() const;

 protected:
  // The container for the WebUI WebView.
  views::View* GetUIContainer() const;

  // Sets the webview content reference.
  void SetWebUIContent(
      std::unique_ptr<OmniboxPopupWebUIBaseContent> webui_content);

  void EnsureWidgetCreated();

  // Called when the widget has just been destroyed.
  virtual void WidgetDestroyed() {}

  // Returns whether or not the popup should include the location bar cutout.
  virtual bool ShouldShowLocationBarCutout() const;

  // Returns whether the WebUI content view receive focus.
  virtual bool ShouldReceiveFocus() const;

  LocationBarView* location_bar_view() const {
    return location_bar_view_.get();
  }

  views::Widget* GetWidget() const { return widget_.get(); }

  // The height of the popup content. Can be 0 if not specified.
  int content_height_ = 0;

 private:
  friend class OmniboxPopupViewWebUITest;
  friend class OmniboxWebUiInteractiveTest;

  // Synchronize the popup widget's bounds to its anchor (location bar view).
  void SynchronizePopupBounds();

  void OnWidgetClosed(views::Widget::ClosedReason closed_reason);

  // Remove observation and reset widget, optionally requesting it to close.
  void ReleaseWidget();

  // Returns the frame view of the widget if it exists. CHECKs if no widget
  // created
  RoundedOmniboxResultsFrame* GetResultsFrame() const;

  // The location bar view that owns `this`.
  const raw_ptr<LocationBarView> location_bar_view_;

  // The container for both the WebUI suggestions list and other WebUI
  // containers
  std::unique_ptr<views::View> owned_omnibox_popup_webui_container_;

  // The WebUI content WebView. Owned by the container.
  raw_ptr<OmniboxPopupWebUIBaseContent> omnibox_popup_webui_content_ = nullptr;

  // The popup widget that contains this WebView. Created and closed by `this`;
  // owned and destroyed by the OS.
  std::unique_ptr<views::Widget> widget_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_PRESENTER_BASE_H_
