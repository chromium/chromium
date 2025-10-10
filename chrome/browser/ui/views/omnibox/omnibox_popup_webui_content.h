// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_WEBUI_CONTENT_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_WEBUI_CONTENT_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/webui/searchbox/webui_omnibox_handler.h"
#include "content/public/browser/render_frame_host.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget.h"

class LocationBarView;
class OmniboxController;
class OmniboxPopupPresenter;

// The content WebView for the popup of a WebUI Omnibox.
class OmniboxPopupWebUIContent : public views::WebView {
  METADATA_HEADER(OmniboxPopupWebUIContent, views::WebView)
 public:
  OmniboxPopupWebUIContent() = delete;
  OmniboxPopupWebUIContent(OmniboxPopupPresenter* presenter,
                           LocationBarView* location_bar_view,
                           OmniboxController* controller,
                           bool include_location_bar_cutout);
  OmniboxPopupWebUIContent(const OmniboxPopupWebUIContent&) = delete;
  OmniboxPopupWebUIContent& operator=(const OmniboxPopupWebUIContent&) = delete;
  ~OmniboxPopupWebUIContent() override;

  // views::View:
  void AddedToWidget() override;

  // content::WebContentsDelegate:
  void ResizeDueToAutoResize(content::WebContents* source,
                             const gfx::Size& new_size) override;
  bool HandleKeyboardEvent(content::WebContents* source,
                           const input::NativeWebKeyboardEvent& event) override;

 private:
  raw_ptr<LocationBarView> location_bar_view_ = nullptr;
  raw_ptr<OmniboxPopupPresenter> omnibox_popup_presenter_ = nullptr;
  // The controller for the Omnibox.
  raw_ptr<OmniboxController> controller_ = nullptr;

  // Whether or not the WebUI popup includes the `location_bar_view` cutout.
  bool include_location_bar_cutout_ = true;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_WEBUI_CONTENT_H_
