// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_PRESENTER_BASE_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_PRESENTER_BASE_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/searchbox/webui_omnibox_handler.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget.h"

class LocationBarView;
class OmniboxController;
class OmniboxPopupWebUIContent;

// A base assistant class for OmniboxPopupViewWebUI, this manages "n" WebViews
// and a Widget to present the WebUI. This class is an implementation detail and
// is not expected to grow or change much with omnibox changes.  The concern of
// this class is presentation only, i.e. Views and Widgets.  For omnibox logic
// concerns and communication between native omnibox code and the WebUI code,
// work with OmniboxPopupViewWebUI directly.
class OmniboxPopupPresenterBase {
 public:
  explicit OmniboxPopupPresenterBase(LocationBarView* location_bar_view);
  OmniboxPopupPresenterBase(const OmniboxPopupPresenterBase&) = delete;
  OmniboxPopupPresenterBase& operator=(const OmniboxPopupPresenterBase&) =
      delete;
  virtual ~OmniboxPopupPresenterBase();

  // Show or hide the popup widget with web view.
  void Show(bool ai_mode);
  void Hide();

  // Tells whether the popup widget exists.
  bool IsShown() const;

  virtual std::optional<size_t> GetShowingWebUIContentIndex() const;

  void SetWidgetContentHeight(int content_height);

 protected:
  views::View* GetOmniboxPopupWebUIContainer() const;

  // Add a new OmniboxPopupWebUIContent view navigated to the given URL. This is
  // inserted into the WebUI container.
  OmniboxPopupWebUIContent* AddOmniboxPopupWebUIContent(
      OmniboxController* controller,
      std::string_view content_url,
      bool include_location_bar_cutout,
      bool wants_focus);

  // Returns the currently "active" Popup content, whichever one is visible or
  // going to be visible within the popup.
  OmniboxPopupWebUIContent* GetActivePopupWebUIContent() const;

  // Show the index'th child view within the WebUI container
  virtual void ShowWebUIContent(size_t index) = 0;

  // Create the Widget if not already created. Returns true if widget was just
  // created.
  bool EnsureWidgetCreated();

  // Called when the widget has just been destroyed.
  virtual void WidgetDestroyed();

 private:
  friend class OmniboxPopupViewWebUITest;

  void OnWidgetClosed(views::Widget::ClosedReason closed_reason);

  // Remove observation and reset widget, optionally requesting it to close.
  void ReleaseWidget();

  // The location bar view that owns `this`.
  const raw_ptr<LocationBarView> location_bar_view_;

  // The container for both the WebUI suggestsions list and other WebUI
  // containers
  std::unique_ptr<views::View> owned_omnibox_popup_webui_container_;

  // The popup widget that contains this WebView. Created and closed by `this`;
  // owned and destroyed by the OS.
  std::unique_ptr<views::Widget> widget_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_PRESENTER_BASE_H_
