// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_WEBUI_BASE_CONTENT_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_WEBUI_BASE_CONTENT_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view_observer.h"

class LocationBarView;
class OmniboxContextMenu;
class OmniboxController;
class OmniboxPopupPresenterBase;
class OmniboxPopupUI;

namespace content {
class WebContents;
}  // namespace content

namespace ui {
class MenuModel;
}  // namespace ui

// The content WebView for the popup of a WebUI Omnibox.
class OmniboxPopupWebUIBaseContent : public views::WebView,
                                     public views::ViewObserver,
                                     public WebUIContentsWrapper::Host {
  METADATA_HEADER(OmniboxPopupWebUIBaseContent, views::WebView)

 public:
  OmniboxPopupWebUIBaseContent() = delete;
  OmniboxPopupWebUIBaseContent(OmniboxPopupPresenterBase* presenter,
                               LocationBarView* location_bar_view,
                               OmniboxController* controller,
                               bool top_rounded_corners);
  OmniboxPopupWebUIBaseContent(const OmniboxPopupWebUIBaseContent&) = delete;
  OmniboxPopupWebUIBaseContent& operator=(const OmniboxPopupWebUIBaseContent&) =
      delete;
  ~OmniboxPopupWebUIBaseContent() override;

  WebUIContentsWrapperT<OmniboxPopupUI>* contents_wrapper() {
    return contents_wrapper_.get();
  }

  // views::View:
  void AddedToWidget() override;

  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* observed_view) override;

  // WebUIContentsWrapper::Host:
  void CloseUI() override;
  void ShowUI() override;
  void ShowCustomContextMenu(
      gfx::Point point,
      std::unique_ptr<ui::MenuModel> menu_model) override;
  void ResizeDueToAutoResize(content::WebContents* source,
                             const gfx::Size& new_size) override;
  bool HandleKeyboardEvent(content::WebContents* source,
                           const input::NativeWebKeyboardEvent& event) override;

  // Notifies the page the widget was hidden.
  virtual void OnPopupHidden();

  // Returns the WebContents from within the wrapper. Don't use
  // GetWebContents() since that may be nullptr if the popup isn't visible.
  content::WebContents* GetWrappedWebContents();

 protected:
  // Callback for cleaning up the `context_menu_` field.
  void OnMenuClosed();

  // Set up the WebUI content page and hook up the Omnibox handlers.
  void SetContentURL(std::string_view url);

  OmniboxController* controller() { return controller_.get(); }

  LocationBarView* location_bar_view() { return location_bar_view_.get(); }

  bool top_rounded_corners() const { return top_rounded_corners_; }

 private:
  // Loads the WebUI content using the cached `content_url`. Creates a new
  // content wrapper (also destroying previous one if it exists) and initializes
  // the renderer.
  void LoadContent();

  raw_ptr<OmniboxPopupPresenterBase> popup_presenter_ = nullptr;
  raw_ptr<LocationBarView> location_bar_view_ = nullptr;
  raw_ptr<OmniboxPopupPresenterBase> omnibox_popup_presenter_ = nullptr;
  // The controller for the Omnibox.
  raw_ptr<OmniboxController> controller_ = nullptr;

  // Whether or not the top of the view has rounded corners.
  bool top_rounded_corners_ = true;

  std::unique_ptr<WebUIContentsWrapperT<OmniboxPopupUI>> contents_wrapper_;
  std::unique_ptr<OmniboxContextMenu> context_menu_;

  // The URL used to load the WebUI. Cached here so the content can be reloaded
  // if the renderer crashes.
  GURL content_url_;

  // Tracks the visible state of the WebUI. This is distinct from the
  // View's visibility (GetVisible()) to handle lifecycle timing differences.
  bool is_shown_ = false;

  // A handler to handle unhandled keyboard messages coming back from the
  // renderer process.
  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;

  base::WeakPtrFactory<OmniboxPopupWebUIBaseContent> weak_factory_{this};
};

BEGIN_VIEW_BUILDER(/* no export */,
                   OmniboxPopupWebUIBaseContent,
                   views::WebView)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(/* no export */, OmniboxPopupWebUIBaseContent)

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_WEBUI_BASE_CONTENT_H_
