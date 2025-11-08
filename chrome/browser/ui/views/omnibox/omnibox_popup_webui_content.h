// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_WEBUI_CONTENT_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_WEBUI_CONTENT_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/browser/ui/webui/searchbox/webui_omnibox_handler.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "content/public/browser/render_frame_host.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget.h"

class LocationBarView;
class OmniboxContextMenu;
class OmniboxController;
class OmniboxPopupPresenterBase;

namespace ui {
class MenuModel;
}  // namespace ui

// The content WebView for the popup of a WebUI Omnibox.
class OmniboxPopupWebUIContent : public views::WebView,
                                 public views::ViewObserver,
                                 public WebUIContentsWrapper::Host {
  METADATA_HEADER(OmniboxPopupWebUIContent, views::WebView)
 public:
  OmniboxPopupWebUIContent() = delete;
  OmniboxPopupWebUIContent(OmniboxPopupPresenterBase* presenter,
                           LocationBarView* location_bar_view,
                           OmniboxController* controller,
                           std::string_view content_url,
                           bool include_location_bar_cutout,
                           bool wants_focus);
  OmniboxPopupWebUIContent(const OmniboxPopupWebUIContent&) = delete;
  OmniboxPopupWebUIContent& operator=(const OmniboxPopupWebUIContent&) = delete;
  ~OmniboxPopupWebUIContent() override;

  WebUIContentsWrapperT<OmniboxPopupUI>* contents_wrapper() {
    return contents_wrapper_.get();
  }

  bool include_location_bar_cutout() const {
    return include_location_bar_cutout_;
  }

  bool wants_focus() const { return wants_focus_; }

  // views::View:
  void AddedToWidget() override;

  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* observed_view) override;

  // WebUIContentsWrapper::Host:
  void ShowUI() override;
  void CloseUI() override;
  void ShowCustomContextMenu(
      gfx::Point point,
      std::unique_ptr<ui::MenuModel> menu_model) override;
  void ResizeDueToAutoResize(content::WebContents* source,
                             const gfx::Size& new_size) override;
  bool HandleKeyboardEvent(content::WebContents* source,
                           const input::NativeWebKeyboardEvent& event) override;

 private:
  raw_ptr<LocationBarView> location_bar_view_ = nullptr;
  raw_ptr<OmniboxPopupPresenterBase> omnibox_popup_presenter_ = nullptr;
  // The controller for the Omnibox.
  raw_ptr<OmniboxController> controller_ = nullptr;

  // Whether or not the WebUI popup includes the `location_bar_view` cutout.
  bool include_location_bar_cutout_ = true;

  // Indicate whether this WebUI content wants to receive activation and focus.
  bool wants_focus_ = false;

  std::unique_ptr<WebUIContentsWrapperT<OmniboxPopupUI>> contents_wrapper_;
  std::unique_ptr<OmniboxContextMenu> context_menu_;

  base::WeakPtrFactory<OmniboxPopupWebUIContent> weak_factory_{this};
};

BEGIN_VIEW_BUILDER(/* no export */, OmniboxPopupWebUIContent, views::WebView)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(/* no export */, OmniboxPopupWebUIContent)

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_WEBUI_CONTENT_H_
